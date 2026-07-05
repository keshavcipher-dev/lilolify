import React, { useState } from 'react';
import { Download, Sliders, Sparkles, FileText, CheckCircle, RefreshCw } from 'lucide-react';
import * as pdfjsLib from 'pdfjs-dist/build/pdf';
import { PDFDocument } from 'pdf-lib';
import DropZone from '../components/DropZone';

// Set worker Src from CDN to ensure zero worker bundle config bugs
pdfjsLib.GlobalWorkerOptions.workerSrc = `https://cdnjs.cloudflare.com/ajax/libs/pdf.js/3.11.174/pdf.worker.min.js`;

export default function Compressor() {
  const [file, setFile] = useState(null);
  const [compressionPreset, setCompressionPreset] = useState('medium'); // 'low', 'medium', 'high', 'custom'
  const [customDpi, setCustomDpi] = useState(150);
  const [customQuality, setCustomQuality] = useState(0.7);
  
  const [processing, setProcessing] = useState(false);
  const [progress, setProgress] = useState({ current: 0, total: 0, stage: '' });
  const [compressedFile, setCompressedFile] = useState(null);
  const [stats, setStats] = useState(null);

  const getCompressionSettings = () => {
    switch (compressionPreset) {
      case 'low': // Big size, maximum quality
        return { dpi: 220, quality: 0.9, label: 'High Quality (Low Compression)' };
      case 'high': // Tiny size, low quality
        return { dpi: 96, quality: 0.45, label: 'Smallest Size (High Compression)' };
      case 'medium': // Standard balance
      default:
        return { dpi: 150, quality: 0.7, label: 'Balanced (Medium Compression)' };
    }
  };

  const handleCompress = async () => {
    if (!file) return;

    setProcessing(true);
    setCompressedFile(null);
    setStats(null);
    
    let dpi = customDpi;
    let quality = customQuality;

    if (compressionPreset !== 'custom') {
      const settings = getCompressionSettings();
      dpi = settings.dpi;
      quality = settings.quality;
    }

    try {
      setProgress({ current: 0, total: 1, stage: 'Reading PDF metadata...' });
      
      const fileReader = new FileReader();
      const fileBytes = await new Promise((resolve) => {
        fileReader.onload = () => resolve(new Uint8Array(fileReader.result));
        fileReader.readAsArrayBuffer(file);
      });

      // Load PDF
      const pdf = await pdfjsLib.getDocument({ data: fileBytes }).promise;
      const numPages = pdf.numPages;
      setProgress({ current: 0, total: numPages, stage: 'Preparing document structure...' });

      // Create new PDF
      const pdfDoc = await PDFDocument.create();

      for (let i = 1; i <= numPages; i++) {
        setProgress({ 
          current: i, 
          total: numPages, 
          stage: `Optimizing page ${i} of ${numPages}...` 
        });

        const page = await pdf.getPage(i);
        
        // Scale factor: PDF uses 72 points per inch. Scale = DPI / 72.
        const scale = dpi / 72;
        const viewport = page.getViewport({ scale });
        
        // Render page onto a canvas
        const canvas = document.createElement('canvas');
        canvas.width = viewport.width;
        canvas.height = viewport.height;
        const canvasContext = canvas.getContext('2d');

        await page.render({ canvasContext, viewport }).promise;

        // Convert canvas image to compressed JPEG arraybuffer
        const jpegUrl = canvas.toDataURL('image/jpeg', quality);
        const jpegBytes = await fetch(jpegUrl).then(res => res.arrayBuffer());

        // Embed in new pdf
        const embeddedImage = await pdfDoc.embedJpg(jpegBytes);
        const newPage = pdfDoc.addPage([viewport.width / scale, viewport.height / scale]); // Reset to normal physical dimensions
        
        newPage.drawImage(embeddedImage, {
          x: 0,
          y: 0,
          width: viewport.width / scale,
          height: viewport.height / scale,
        });
      }

      setProgress({ current: numPages, total: numPages, stage: 'Writing compressed output stream...' });
      const compressedPdfBytes = await pdfDoc.save();
      
      const blob = new Blob([compressedPdfBytes], { type: 'application/pdf' });
      const compressedSize = blob.size;

      setCompressedFile(blob);
      setStats({
        originalSize: file.size,
        compressedSize: compressedSize,
        savings: ((file.size - compressedSize) / file.size * 100).toFixed(1)
      });
    } catch (err) {
      console.error('Error during compression:', err);
      alert('An error occurred during PDF compression. Check console for details.');
    } finally {
      setProcessing(false);
    }
  };

  const handleDownload = () => {
    if (!compressedFile) return;
    const url = URL.createObjectURL(compressedFile);
    const link = document.createElement('a');
    link.href = url;
    const originalName = file.name.replace(/\.[^/.]+$/, "");
    link.download = `${originalName}_compressed.pdf`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  const formatSize = (bytes) => {
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
  };

  return (
    <div className="w-full flex flex-col items-center py-6 px-4 animate-fade-in">
      <div className="text-center max-w-xl mb-8">
        <h2 className="font-display font-extrabold text-3xl mb-2 text-transparent bg-clip-text bg-gradient-to-r from-purple-400 via-indigo-200 to-cyan-400">
          Targeted PDF Compression
        </h2>
        <p className="text-sm text-text-muted">
          Reduce your PDF size client-side. Our smart compressor maintains clean readable vector texts while optimizing hidden high-resolution media artifacts.
        </p>
      </div>

      <DropZone onFilesSelected={setFile} />

      {file && (
        <div className="w-full max-w-2xl mt-8 space-y-6">
          {/* Settings Card */}
          <div className="glass-panel p-6 flex flex-col space-y-5">
            <h3 className="font-display font-bold text-lg text-white flex items-center gap-2">
              <Sliders size={20} className="text-purple-400" />
              Compression Options
            </h3>
            
            {/* Presets Grid */}
            <div className="grid grid-cols-4 gap-3">
              {[
                { id: 'low', label: 'Low', desc: 'Highest Quality' },
                { id: 'medium', label: 'Medium', desc: 'Balanced Size' },
                { id: 'high', label: 'High', desc: 'Max Compress' },
                { id: 'custom', label: 'Custom', desc: 'Set DPI / Qual' },
              ].map((preset) => (
                <button
                  key={preset.id}
                  type="button"
                  onClick={() => setCompressionPreset(preset.id)}
                  className={`flex flex-col items-center justify-center p-3 rounded-lg border text-center transition-all ${
                    compressionPreset === preset.id
                      ? 'border-cyan-400 bg-cyan-950/20 text-cyan-200 shadow-[0_0_10px_rgba(6,182,212,0.15)]'
                      : 'border-white/5 bg-white/5 text-text-muted hover:border-white/10 hover:bg-white/10'
                  }`}
                >
                  <span className="font-display font-semibold text-sm">{preset.label}</span>
                  <span className="text-[10px] opacity-75 mt-0.5">{preset.desc}</span>
                </button>
              ))}
            </div>

            {/* Custom Sliders */}
            {compressionPreset === 'custom' && (
              <div className="space-y-4 pt-2 border-t border-white/5 animate-fade-in">
                <div className="flex flex-col space-y-2">
                  <div className="flex justify-between text-xs font-semibold">
                    <span className="text-text-muted">Target Resolution (DPI)</span>
                    <span className="text-cyan-400">{customDpi} DPI</span>
                  </div>
                  <input
                    type="range"
                    min="72"
                    max="300"
                    step="1"
                    value={customDpi}
                    onChange={(e) => setCustomDpi(parseInt(e.target.value))}
                    className="w-full"
                  />
                  <div className="flex justify-between text-[10px] text-text-dark">
                    <span>72 (Rough text)</span>
                    <span>150 (Web Standard)</span>
                    <span>300 (Print Standard)</span>
                  </div>
                </div>

                <div className="flex flex-col space-y-2">
                  <div className="flex justify-between text-xs font-semibold">
                    <span className="text-text-muted">Image Quality (JPEG Factor)</span>
                    <span className="text-purple-400">{Math.round(customQuality * 100)}%</span>
                  </div>
                  <input
                    type="range"
                    min="0.1"
                    max="1"
                    step="0.05"
                    value={customQuality}
                    onChange={(e) => setCustomQuality(parseFloat(e.target.value))}
                    className="w-full"
                  />
                  <div className="flex justify-between text-[10px] text-text-dark">
                    <span>10% (Grainy)</span>
                    <span>70% (Great Balance)</span>
                    <span>100% (Lossless Image)</span>
                  </div>
                </div>
              </div>
            )}

            {/* Submit Button */}
            {!processing && !stats && (
              <button
                type="button"
                onClick={handleCompress}
                className="btn-primary w-full justify-center"
              >
                <Sparkles size={18} />
                Shoot and Shorten Size
              </button>
            )}

            {/* Process Logger */}
            {processing && (
              <div className="flex flex-col space-y-3 pt-2">
                <div className="flex items-center justify-between text-xs text-text-muted">
                  <span className="flex items-center gap-2">
                    <RefreshCw size={14} className="spinner text-cyan-400" />
                    {progress.stage}
                  </span>
                  <span>{progress.current}/{progress.total}</span>
                </div>
                <div className="w-full bg-white/5 h-2 rounded-full overflow-hidden">
                  <div 
                    className="bg-gradient-to-r from-purple-500 to-cyan-400 h-full transition-all duration-300"
                    style={{ width: `${(progress.current / progress.total) * 100}%` }}
                  />
                </div>
              </div>
            )}

            {/* Completion Stats */}
            {stats && (
              <div className="p-5 bg-emerald-950/20 border border-emerald-500/20 rounded-xl space-y-4 animate-fade-in">
                <div className="flex items-center gap-2 text-emerald-400 font-display font-semibold">
                  <CheckCircle size={20} />
                  Compression Successful!
                </div>
                
                <div className="grid grid-cols-3 gap-2 text-center text-xs">
                  <div className="bg-white/5 p-2.5 rounded-lg border border-white/5">
                    <p className="text-text-muted mb-0.5">Original Size</p>
                    <p className="font-mono text-sm font-bold text-white">{formatSize(stats.originalSize)}</p>
                  </div>
                  <div className="bg-white/5 p-2.5 rounded-lg border border-white/5">
                    <p className="text-text-muted mb-0.5">Compressed Size</p>
                    <p className="font-mono text-sm font-bold text-cyan-400">{formatSize(stats.compressedSize)}</p>
                  </div>
                  <div className="bg-white/5 p-2.5 rounded-lg border border-white/5">
                    <p className="text-text-muted mb-0.5">Total Savings</p>
                    <p className="font-mono text-sm font-bold text-purple-400">-{stats.savings}%</p>
                  </div>
                </div>

                <div className="flex gap-3 pt-2">
                  <button
                    type="button"
                    onClick={handleDownload}
                    className="btn-primary flex-1 justify-center py-2.5"
                  >
                    <Download size={18} />
                    Download Compressed PDF
                  </button>
                  <button
                    type="button"
                    onClick={() => {
                      setStats(null);
                      setCompressedFile(null);
                    }}
                    className="btn-secondary py-2.5"
                  >
                    Reset
                  </button>
                </div>
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  );
}
