import React, { useState } from 'react';
import { RefreshCw, FileText, Download, CheckCircle, AlertCircle, FileSpreadsheet, Image, ArrowLeftRight } from 'lucide-react';
import * as pdfjsLib from 'pdfjs-dist/build/pdf';
import { PDFDocument } from 'pdf-lib';
import { Document, Packer, Paragraph, TextRun } from 'docx';
import PptxGenJS from 'pptxgenjs';
import DropZone from '../components/DropZone';

pdfjsLib.GlobalWorkerOptions.workerSrc = `https://cdnjs.cloudflare.com/ajax/libs/pdf.js/3.11.174/pdf.worker.min.js`;

export default function Converter() {
  const [conversionType, setConversionType] = useState('pdf2docx'); // pdf2docx, pdf2pptx, pdf2images, images2pdf
  
  // States
  const [pdfFile, setPdfFile] = useState(null);
  const [imageFiles, setImageFiles] = useState([]);
  const [converting, setConverting] = useState(false);
  const [progress, setProgress] = useState({ current: 0, total: 0, stage: '' });
  const [convertedResult, setConvertedResult] = useState(null); // Blob for download
  const [downloadName, setDownloadName] = useState('');
  const [error, setError] = useState(null);

  // --- PDF TO DOCX ---
  const convertPdfToDocx = async (file) => {
    try {
      setProgress({ current: 0, total: 100, stage: 'Reading file buffer...' });
      const fileBytes = new Uint8Array(await file.arrayBuffer());
      const pdf = await pdfjsLib.getDocument({ data: fileBytes }).promise;
      const numPages = pdf.numPages;

      const paragraphs = [];
      paragraphs.push(
        new Paragraph({
          children: [
            new TextRun({
              text: `Converted from: ${file.name}`,
              bold: true,
              size: 28,
              color: '4F46E5',
            }),
          ],
          spacing: { after: 200 },
        })
      );

      for (let i = 1; i <= numPages; i++) {
        setProgress({ current: i, total: numPages, stage: `Extracting text content from page ${i}...` });
        const page = await pdf.getPage(i);
        const textContent = await page.getTextContent();
        
        let lastY = null;
        let lineText = '';

        for (const item of textContent.items) {
          const currentY = item.transform[5];
          if (lastY !== null && Math.abs(currentY - lastY) > 8) {
            if (lineText.trim()) {
              paragraphs.push(
                new Paragraph({
                  children: [new TextRun({ text: lineText.trim(), size: 22 })],
                  spacing: { after: 120 }
                })
              );
            }
            lineText = '';
          }
          lineText += ' ' + item.str;
          lastY = currentY;
        }
        
        if (lineText.trim()) {
          paragraphs.push(
            new Paragraph({
              children: [new TextRun({ text: lineText.trim(), size: 22 })],
              spacing: { after: 120 }
            })
          );
        }

        // Add page break between pages
        if (i < numPages) {
          paragraphs.push(
            new Paragraph({
              children: [new TextRun({ break: 1 })]
            })
          );
        }
      }

      setProgress({ current: 95, total: 100, stage: 'Building Word XML assets...' });
      
      const doc = new Document({
        sections: [{
          properties: {},
          children: paragraphs,
        }],
      });

      const blob = await Packer.toBlob(doc);
      setConvertedResult(blob);
      setDownloadName(file.name.replace(/\.[^/.]+$/, "") + ".docx");
    } catch (err) {
      console.error(err);
      throw new Error("Unable to parse and reconstruct document text structure.");
    }
  };

  // --- PDF TO PPTX ---
  const convertPdfToPptx = async (file) => {
    try {
      setProgress({ current: 0, total: 100, stage: 'Initializing PowerPoint engine...' });
      const fileBytes = new Uint8Array(await file.arrayBuffer());
      const pdf = await pdfjsLib.getDocument({ data: fileBytes }).promise;
      const numPages = pdf.numPages;

      const pptx = new PptxGenJS();
      pptx.layout = 'LAYOUT_16x9';

      for (let i = 1; i <= numPages; i++) {
        setProgress({ current: i, total: numPages, stage: `Transferring layers from page ${i}...` });
        const page = await pdf.getPage(i);
        const textContent = await page.getTextContent();
        
        const slide = pptx.addSlide();
        slide.background = { fill: '0D0A21' }; // Match dark theme

        // Add slide footer
        slide.addText(`Slide ${i} of ${numPages} — pdfshooter`, {
          x: 0.5,
          y: 7.0,
          w: 12.33,
          h: 0.4,
          fontSize: 10,
          color: '9CA3AF'
        });

        // Group page text
        const textRuns = textContent.items.map(item => item.str).join(' ');
        
        if (textRuns.trim()) {
          slide.addText(textRuns.trim(), {
            x: 0.75,
            y: 0.75,
            w: 11.83,
            h: 5.8,
            fontSize: 13,
            color: 'F3F4F6',
            valign: 'top',
            margin: 10
          });
        } else {
          // If page has no text (e.g. image-only PDF), render page to slide as image
          const scale = 1.5;
          const viewport = page.getViewport({ scale });
          const canvas = document.createElement('canvas');
          canvas.width = viewport.width;
          canvas.height = viewport.height;
          const ctx = canvas.getContext('2d');
          await page.render({ canvasContext: ctx, viewport }).promise;
          
          const imgData = canvas.toDataURL('image/jpeg', 0.85);
          slide.addImage({ data: imgData, x: 0.5, y: 0.5, w: 12.33, h: 6.0 });
        }
      }

      setProgress({ current: 95, total: 100, stage: 'Formatting presentation package...' });
      const pptxBlob = await pptx.write('blob');
      setConvertedResult(pptxBlob);
      setDownloadName(file.name.replace(/\.[^/.]+$/, "") + ".pptx");
    } catch (err) {
      console.error(err);
      throw new Error("Unable to create presentation layout from PDF.");
    }
  };

  // --- PDF TO IMAGES ---
  const convertPdfToImages = async (file) => {
    try {
      setProgress({ current: 0, total: 100, stage: 'Rendering pages to raster assets...' });
      const fileBytes = new Uint8Array(await file.arrayBuffer());
      const pdf = await pdfjsLib.getDocument({ data: fileBytes }).promise;
      const numPages = pdf.numPages;

      const pdfDoc = await PDFDocument.create();

      // Render first page as JPG output example or convert all pages to high quality JPG compiled pages
      // For images, we will generate a high-res combined layout or download the first page
      // To keep it simple and clean, we render the pages as JPEG embeds into a compressed gallery zip, 
      // or we let them download the pages as a single high-quality consolidated image. Let's make it convert pages to image-based PDF (which rasterizes all fonts/vectors, resolving format validation issues on job/banking websites).
      for (let i = 1; i <= numPages; i++) {
        setProgress({ current: i, total: numPages, stage: `Rasterizing page ${i}...` });
        const page = await pdf.getPage(i);
        const viewport = page.getViewport({ scale: 2.0 }); // 2x scale for high resolution
        const canvas = document.createElement('canvas');
        canvas.width = viewport.width;
        canvas.height = viewport.height;
        const ctx = canvas.getContext('2d');
        await page.render({ canvasContext: ctx, viewport }).promise;
        
        const imgData = canvas.toDataURL('image/jpeg', 0.9);
        const imgBytes = await fetch(imgData).then(res => res.arrayBuffer());
        const embeddedImg = await pdfDoc.embedJpg(imgBytes);
        const newPage = pdfDoc.addPage([viewport.width / 2, viewport.height / 2]);
        newPage.drawImage(embeddedImg, {
          x: 0,
          y: 0,
          width: viewport.width / 2,
          height: viewport.height / 2
        });
      }

      setProgress({ current: 95, total: 100, stage: 'Building flat PDF image stream...' });
      const bytes = await pdfDoc.save();
      const blob = new Blob([bytes], { type: 'application/pdf' });
      setConvertedResult(blob);
      setDownloadName(file.name.replace(/\.[^/.]+$/, "") + "_rasterized.pdf");
    } catch (err) {
      console.error(err);
      throw new Error("Unable to rasterize PDF elements.");
    }
  };

  // --- IMAGES TO PDF ---
  const convertImagesToPdf = async (files) => {
    try {
      setProgress({ current: 0, total: files.length, stage: 'Creating blank PDF structure...' });
      const pdfDoc = await PDFDocument.create();

      for (let i = 0; i < files.length; i++) {
        const file = files[i];
        setProgress({ current: i + 1, total: files.length, stage: `Embedding image ${file.name}...` });
        const bytes = await file.arrayBuffer();
        
        let image;
        if (file.type === 'image/png' || file.name.toLowerCase().endsWith('.png')) {
          image = await pdfDoc.embedPng(bytes);
        } else {
          image = await pdfDoc.embedJpg(bytes);
        }

        // Fit page to image size
        const page = pdfDoc.addPage([image.width, image.height]);
        page.drawImage(image, {
          x: 0,
          y: 0,
          width: image.width,
          height: image.height
        });
      }

      setProgress({ current: files.length, total: files.length, stage: 'Exporting binary PDF output...' });
      const pdfBytes = await pdfDoc.save();
      const blob = new Blob([pdfBytes], { type: 'application/pdf' });
      setConvertedResult(blob);
      setDownloadName("images_combined.pdf");
    } catch (err) {
      console.error(err);
      throw new Error("Failed to compile image layers. Make sure files are PNG/JPEG.");
    }
  };

  const handleStartConversion = async () => {
    setError(null);
    setConvertedResult(null);

    if (conversionType === 'images2pdf') {
      if (imageFiles.length === 0) {
        setError("Please upload at least one image file.");
        return;
      }
      setConverting(true);
      try {
        await convertImagesToPdf(imageFiles);
      } catch (err) {
        setError(err.message || 'An error occurred during image packing.');
      } finally {
        setConverting(false);
      }
    } else {
      if (!pdfFile) {
        setError("Please upload a PDF file first.");
        return;
      }
      setConverting(true);
      try {
        if (conversionType === 'pdf2docx') {
          await convertPdfToDocx(pdfFile);
        } else if (conversionType === 'pdf2pptx') {
          await convertPdfToPptx(pdfFile);
        } else if (conversionType === 'pdf2images') {
          await convertPdfToImages(pdfFile);
        }
      } catch (err) {
        setError(err.message || 'Conversion failed. Check layout formatting.');
      } finally {
        setConverting(false);
      }
    }
  };

  const handleDownload = () => {
    if (!convertedResult) return;
    const url = URL.createObjectURL(convertedResult);
    const link = document.createElement('a');
    link.href = url;
    link.download = downloadName;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  const resetAll = () => {
    setPdfFile(null);
    setImageFiles([]);
    setConvertedResult(null);
    setError(null);
    setProgress({ current: 0, total: 0, stage: '' });
  };

  return (
    <div className="w-full flex flex-col items-center py-6 px-4 animate-fade-in">
      <div className="text-center max-w-xl mb-8">
        <h2 className="font-display font-extrabold text-3xl mb-2 text-transparent bg-clip-text bg-gradient-to-r from-purple-400 via-indigo-200 to-cyan-400">
          Document Converter
        </h2>
        <p className="text-sm text-text-muted">
          Transform your files seamlessly. Convert PDF documents to Microsoft Word, PowerPoint slides, high-res raster pages, or compile images into a single PDF.
        </p>
      </div>

      {/* Select Conversion Direction */}
      <div className="grid grid-cols-4 gap-3 w-full max-w-2xl mb-8">
        {[
          { id: 'pdf2docx', label: 'PDF to Word', icon: <FileText size={16} /> },
          { id: 'pdf2pptx', label: 'PDF to Slide', icon: <FileSpreadsheet size={16} /> },
          { id: 'pdf2images', label: 'Rasterize PDF', icon: <Image size={16} /> },
          { id: 'images2pdf', label: 'Images to PDF', icon: <ArrowLeftRight size={16} /> },
        ].map((type) => (
          <button
            key={type.id}
            type="button"
            onClick={() => {
              setConversionType(type.id);
              resetAll();
            }}
            className={`flex flex-col items-center justify-center p-4 rounded-xl border text-center transition-all ${
              conversionType === type.id
                ? 'border-purple-400 bg-purple-950/20 text-purple-200 shadow-[0_0_15px_rgba(168,85,247,0.15)]'
                : 'border-white/5 bg-white/5 text-text-muted hover:border-white/10 hover:bg-white/10'
            }`}
          >
            <div className="mb-2 text-purple-400">{type.icon}</div>
            <span className="font-display font-semibold text-xs">{type.label}</span>
          </button>
        ))}
      </div>

      {/* Upload Zone */}
      <div className="w-full max-w-2xl">
        {conversionType === 'images2pdf' ? (
          <DropZone
            accept=".png,.jpg,.jpeg"
            multiple={true}
            onFilesSelected={(files) => {
              if (files) {
                const list = Array.isArray(files) ? files : [files];
                setImageFiles(list);
                setConvertedResult(null);
              }
            }}
            title="Upload JPG/PNG images to compile"
            subtitle="Drag & drop multiple images in order"
          />
        ) : (
          <DropZone
            onFilesSelected={(file) => {
              setPdfFile(file);
              setConvertedResult(null);
            }}
            title="Upload PDF document to convert"
            subtitle="Drag & drop or click to upload"
          />
        )}
      </div>

      {/* Conversion execution box */}
      {(pdfFile || imageFiles.length > 0) && (
        <div className="w-full max-w-2xl mt-8">
          <div className="glass-panel p-6 space-y-5">
            <h3 className="font-display font-semibold text-base text-white">
              Ready for Conversion: {conversionType === 'images2pdf' ? `${imageFiles.length} Images` : pdfFile.name}
            </h3>

            {error && (
              <div className="flex items-center gap-2 text-pink-500 text-xs bg-pink-950/20 border border-pink-500/20 py-2.5 px-4 rounded-lg">
                <AlertCircle size={14} />
                <span>{error}</span>
              </div>
            )}

            {!converting && !convertedResult && (
              <button
                type="button"
                onClick={handleStartConversion}
                className="btn-primary w-full justify-center"
              >
                <RefreshCw size={18} />
                Shoot & Convert
              </button>
            )}

            {converting && (
              <div className="space-y-3">
                <div className="flex items-center justify-between text-xs text-cyan-400">
                  <span className="flex items-center gap-2">
                    <RefreshCw size={14} className="spinner" />
                    {progress.stage}
                  </span>
                  <span>{progress.current}/{progress.total}</span>
                </div>
                <div className="w-full bg-white/5 h-1.5 rounded-full overflow-hidden">
                  <div 
                    className="bg-gradient-to-r from-purple-500 to-cyan-400 h-full transition-all duration-300"
                    style={{ width: `${(progress.current / Math.max(progress.total, 1)) * 100}%` }}
                  />
                </div>
              </div>
            )}

            {convertedResult && (
              <div className="p-4 bg-emerald-950/20 border border-emerald-500/20 rounded-xl space-y-3 animate-fade-in">
                <div className="flex items-center gap-2 text-emerald-400 font-display font-semibold text-sm">
                  <CheckCircle size={18} />
                  Conversion Completed! Ready to Download.
                </div>
                <div className="flex gap-3">
                  <button
                    type="button"
                    onClick={handleDownload}
                    className="btn-primary flex-1 justify-center"
                  >
                    <Download size={18} />
                    Download Converted File
                  </button>
                  <button
                    type="button"
                    onClick={resetAll}
                    className="btn-secondary"
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
