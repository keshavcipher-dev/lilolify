import React, { useState } from 'react';
import { Lock, Shield, EyeOff, Edit3, Download, CheckCircle, AlertCircle, RefreshCw } from 'lucide-react';
import { PDFDocument, rgb, degrees, StandardFonts } from 'pdf-lib';
import DropZone from '../components/DropZone';

export default function Protector() {
  const [file, setFile] = useState(null);
  
  // Settings States
  const [enableLock, setEnableLock] = useState(true);
  const [password, setPassword] = useState('');
  
  const [enableWatermark, setEnableWatermark] = useState(false);
  const [watermarkText, setWatermarkText] = useState('CONFIDENTIAL');
  const [watermarkSize, setWatermarkSize] = useState(50);
  const [watermarkOpacity, setWatermarkOpacity] = useState(0.25);
  const [watermarkRotation, setWatermarkRotation] = useState(-45);
  const [watermarkColor, setWatermarkColor] = useState('rgba(168, 85, 247, 0.4)'); // Purple tint

  const [processing, setProcessing] = useState(false);
  const [protectedBlob, setProtectedBlob] = useState(null);
  const [error, setError] = useState(null);

  const applyProtection = async () => {
    if (!file) return;
    setError(null);
    setProtectedBlob(null);

    if (enableLock && !password.trim()) {
      setError("Please input a valid security password.");
      return;
    }

    if (!enableLock && !enableWatermark) {
      setError("Please choose at least one action: Encrypt or Watermark.");
      return;
    }

    setProcessing(true);

    try {
      const fileBytes = new Uint8Array(await file.arrayBuffer());
      const pdfDoc = await PDFDocument.load(fileBytes);
      const pages = pdfDoc.getPages();

      // Apply watermark first
      if (enableWatermark && watermarkText.trim()) {
        const font = await pdfDoc.embedFont(StandardFonts.HelveticaBold);
        
        // Parse custom color rgba values
        let textRgb = rgb(0.5, 0.5, 0.5); // default grey
        if (watermarkColor.startsWith('rgba')) {
          const vals = watermarkColor.replace(/[^\d,.]/g, '').split(',');
          if (vals.length >= 3) {
            textRgb = rgb(parseFloat(vals[0])/255, parseFloat(vals[1])/255, parseFloat(vals[2])/255);
          }
        }

        for (const page of pages) {
          const { width, height } = page.getSize();
          
          // Render centered watermark
          page.drawText(watermarkText, {
            x: width / 2,
            y: height / 2,
            size: watermarkSize,
            font: font,
            color: textRgb,
            opacity: watermarkOpacity,
            rotate: degrees(watermarkRotation),
            xAnchor: 'center',
            yAnchor: 'middle'
          });
        }
      }

      // Apply password encryption
      if (enableLock && password.trim()) {
        // Standard user password locking, owner permissions set to read-only copy restrictions
        pdfDoc.encrypt({
          userPassword: password,
          ownerPassword: 'pdfshooter_admin_security_key',
          permissions: {
            printing: 'highResolution',
            modifying: false,
            copying: false,
            annotating: false,
          }
        });
      }

      const outputBytes = await pdfDoc.save();
      const blob = new Blob([outputBytes], { type: 'application/pdf' });
      setProtectedBlob(blob);
    } catch (err) {
      console.error(err);
      setError("An error occurred during file protection. Ensure file is not already encrypted.");
    } finally {
      setProcessing(false);
    }
  };

  const downloadFile = () => {
    if (!protectedBlob) return;
    const url = URL.createObjectURL(protectedBlob);
    const link = document.createElement('a');
    link.href = url;
    const suffix = (enableLock && enableWatermark) ? '_locked_watermarked' : enableLock ? '_locked' : '_watermarked';
    link.download = `${file.name.replace(/\.[^/.]+$/, "")}${suffix}.pdf`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  const hexToRgbString = (hex, alpha = 0.4) => {
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    return `rgba(${r}, ${g}, ${b}, ${alpha})`;
  };

  return (
    <div className="w-full flex flex-col items-center py-6 px-4 animate-fade-in">
      <div className="text-center max-w-xl mb-8">
        <h2 className="font-display font-extrabold text-3xl mb-2 text-transparent bg-clip-text bg-gradient-to-r from-purple-400 via-indigo-200 to-cyan-400">
          Protect & Watermark
        </h2>
        <p className="text-sm text-text-muted">
          Add access locks and dynamic overlays. Encrypt files with secure passwords or layer custom visual watermarks across all pages instantly.
        </p>
      </div>

      <DropZone onFilesSelected={setFile} />

      {file && (
        <div className="w-full max-w-2xl mt-8 space-y-6">
          <div className="glass-panel p-6 space-y-6">
            
            {/* Password Encryption Settings */}
            <div className="space-y-4">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-2 font-display font-semibold text-white">
                  <Lock size={18} className="text-purple-400" />
                  Password Encryption
                </div>
                <input
                  type="checkbox"
                  checked={enableLock}
                  onChange={(e) => {
                    setEnableLock(e.target.checked);
                    setProtectedBlob(null);
                  }}
                  className="w-4 h-4 accent-purple-500 rounded cursor-pointer"
                />
              </div>

              {enableLock && (
                <div className="flex flex-col space-y-2 pl-6 animate-fade-in">
                  <span className="text-xs text-text-muted">Set Access Password</span>
                  <input
                    type="password"
                    placeholder="Enter password to encrypt file..."
                    value={password}
                    onChange={(e) => {
                      setPassword(e.target.value);
                      setProtectedBlob(null);
                    }}
                    className="bg-white/5 border border-white/10 rounded-lg p-2.5 text-sm text-white focus:outline-none focus:border-purple-500 font-mono w-full"
                  />
                  <span className="text-[10px] text-text-dark">This password will be required when opening the PDF.</span>
                </div>
              )}
            </div>

            <div className="border-t border-white/5" />

            {/* Watermark Overlay Settings */}
            <div className="space-y-4">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-2 font-display font-semibold text-white">
                  <EyeOff size={18} className="text-cyan-400" />
                  Watermark Overlay
                </div>
                <input
                  type="checkbox"
                  checked={enableWatermark}
                  onChange={(e) => {
                    setEnableWatermark(e.target.checked);
                    setProtectedBlob(null);
                  }}
                  className="w-4 h-4 accent-cyan-500 rounded cursor-pointer"
                />
              </div>

              {enableWatermark && (
                <div className="grid grid-cols-2 gap-4 pl-6 animate-fade-in">
                  <div className="col-span-2 flex flex-col space-y-1.5">
                    <span className="text-xs text-text-muted">Watermark Text</span>
                    <input
                      type="text"
                      placeholder="e.g. CONFIDENTIAL"
                      value={watermarkText}
                      onChange={(e) => {
                        setWatermarkText(e.target.value);
                        setProtectedBlob(null);
                      }}
                      className="bg-white/5 border border-white/10 rounded-lg p-2.5 text-sm text-white focus:outline-none focus:border-cyan-500 font-display"
                    />
                  </div>

                  <div className="flex flex-col space-y-1.5">
                    <span className="text-xs text-text-muted">Font Size ({watermarkSize}px)</span>
                    <input
                      type="range"
                      min="20"
                      max="100"
                      value={watermarkSize}
                      onChange={(e) => {
                        setWatermarkSize(parseInt(e.target.value));
                        setProtectedBlob(null);
                      }}
                    />
                  </div>

                  <div className="flex flex-col space-y-1.5">
                    <span className="text-xs text-text-muted">Opacity ({Math.round(watermarkOpacity * 100)}%)</span>
                    <input
                      type="range"
                      min="0.05"
                      max="0.9"
                      step="0.05"
                      value={watermarkOpacity}
                      onChange={(e) => {
                        setWatermarkOpacity(parseFloat(e.target.value));
                        setProtectedBlob(null);
                      }}
                    />
                  </div>

                  <div className="flex flex-col space-y-1.5">
                    <span className="text-xs text-text-muted">Rotation ({watermarkRotation}°)</span>
                    <input
                      type="range"
                      min="-90"
                      max="90"
                      value={watermarkRotation}
                      onChange={(e) => {
                        setWatermarkRotation(parseInt(e.target.value));
                        setProtectedBlob(null);
                      }}
                    />
                  </div>

                  <div className="flex flex-col space-y-1.5">
                    <span className="text-xs text-text-muted font-medium">Text Color</span>
                    <div className="flex gap-2 items-center">
                      <input
                        type="color"
                        value="#a855f7"
                        onChange={(e) => {
                          setWatermarkColor(hexToRgbString(e.target.value, watermarkOpacity));
                          setProtectedBlob(null);
                        }}
                        className="bg-transparent border-0 w-8 h-8 cursor-pointer rounded-lg overflow-hidden"
                      />
                      <span className="text-xs text-text-muted">Select Color Tone</span>
                    </div>
                  </div>
                </div>
              )}
            </div>

            {/* Execute Operations */}
            <div className="pt-4 border-t border-white/5 flex flex-col gap-4">
              {error && (
                <div className="flex items-center gap-2 text-pink-500 text-xs bg-pink-950/20 border border-pink-500/20 py-2.5 px-4 rounded-lg">
                  <AlertCircle size={14} />
                  <span>{error}</span>
                </div>
              )}

              {!processing && !protectedBlob && (
                <button
                  type="button"
                  onClick={applyProtection}
                  className="btn-primary w-full justify-center"
                >
                  <Shield size={18} />
                  Secure PDF File
                </button>
              )}

              {processing && (
                <div className="flex items-center justify-center gap-2 py-3 text-cyan-400 text-sm">
                  <RefreshCw size={14} className="spinner" />
                  Structuring locks and writing overlays...
                </div>
              )}

              {protectedBlob && (
                <div className="p-4 bg-emerald-950/20 border border-emerald-500/20 rounded-xl space-y-3 animate-fade-in">
                  <div className="flex items-center gap-2 text-emerald-400 font-display font-semibold text-sm">
                    <CheckCircle size={18} />
                    PDF Secured and Ready!
                  </div>
                  <div className="flex gap-3">
                    <button
                      type="button"
                      onClick={downloadFile}
                      className="btn-primary flex-1 justify-center"
                    >
                      <Download size={18} />
                      Download Secured PDF
                    </button>
                    <button
                      type="button"
                      onClick={() => {
                        setProtectedBlob(null);
                        setPassword('');
                      }}
                      className="btn-secondary"
                    >
                      Reset
                    </button>
                  </div>
                </div>
              )}
            </div>

          </div>
        </div>
      )}
    </div>
  );
}
