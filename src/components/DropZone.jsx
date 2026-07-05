import React, { useState, useRef } from 'react';
import { Upload, FileText, AlertCircle, CheckCircle } from 'lucide-react';

export default function DropZone({ 
  onFilesSelected, 
  accept = ".pdf", 
  multiple = false, 
  maxSizeMB = 100, 
  title = "Drag and drop your PDF here",
  subtitle = "Supports files up to 100MB"
}) {
  const [isDragActive, setIsDragActive] = useState(false);
  const [error, setError] = useState(null);
  const [files, setFiles] = useState([]);
  const fileInputRef = useRef(null);

  const formatBytes = (bytes) => {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const dm = 2;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
  };

  const processFiles = (fileList) => {
    setError(null);
    const selected = Array.from(fileList);
    
    // Check type matching
    const acceptedExtensions = accept.split(',').map(ext => ext.trim().toLowerCase());
    const invalidType = selected.find(file => {
      const fileNameLower = file.name.toLowerCase();
      return !acceptedExtensions.some(ext => fileNameLower.endsWith(ext));
    });

    if (invalidType) {
      setError(`Only ${accept} files are supported.`);
      return;
    }

    // Check size matching
    const sizeLimit = maxSizeMB * 1024 * 1024;
    const heavyFile = selected.find(file => file.size > sizeLimit);
    if (heavyFile) {
      setError(`Files cannot exceed ${maxSizeMB}MB.`);
      return;
    }

    setFiles(selected);
    onFilesSelected(multiple ? selected : selected[0]);
  };

  const handleDrag = (e) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === "dragenter" || e.type === "dragover") {
      setIsDragActive(true);
    } else if (e.type === "dragleave") {
      setIsDragActive(false);
    }
  };

  const handleDrop = (e) => {
    e.preventDefault();
    e.stopPropagation();
    setIsDragActive(false);

    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      processFiles(e.dataTransfer.files);
    }
  };

  const handleChange = (e) => {
    e.preventDefault();
    if (e.target.files && e.target.files.length > 0) {
      processFiles(e.target.files);
    }
  };

  const onButtonClick = () => {
    fileInputRef.current.click();
  };

  const removeFiles = () => {
    setFiles([]);
    setError(null);
    onFilesSelected(null);
  };

  return (
    <div className="w-full flex flex-col items-center">
      <div
        className={`w-full max-w-2xl min-h-[220px] flex flex-col items-center justify-center border-2 border-dashed rounded-2xl p-8 text-center cursor-pointer transition-all duration-300 ${
          isDragActive 
            ? 'border-cyan-400 bg-cyan-950/20 scale-[1.01] shadow-[0_0_20px_rgba(6,182,212,0.15)]' 
            : files.length > 0
              ? 'border-purple-500/50 bg-purple-950/5'
              : 'border-white/10 hover:border-purple-500/30 hover:bg-white/5'
        }`}
        onDragEnter={handleDrag}
        onDragOver={handleDrag}
        onDragLeave={handleDrag}
        onDrop={handleDrop}
        onClick={onButtonClick}
      >
        <input
          ref={fileInputRef}
          type="file"
          className="hidden"
          multiple={multiple}
          accept={accept}
          onChange={handleChange}
        />

        {files.length > 0 ? (
          <div className="flex flex-col items-center space-y-4 animate-fade-in" onClick={(e) => e.stopPropagation()}>
            <div className="p-4 bg-purple-500/15 rounded-full text-purple-400">
              <CheckCircle size={40} className="stroke-[1.5]" />
            </div>
            <div>
              <h3 className="font-display font-semibold text-lg text-white">
                {multiple ? `${files.length} Files Selected` : files[0].name}
              </h3>
              <p className="text-sm text-text-muted mt-1">
                {multiple 
                  ? formatBytes(files.reduce((acc, f) => acc + f.size, 0)) 
                  : formatBytes(files[0].size)
                }
              </p>
            </div>
            <div className="flex items-center gap-3">
              <button 
                type="button" 
                className="btn-secondary py-2 px-4 text-xs font-semibold"
                onClick={removeFiles}
              >
                Clear Selection
              </button>
              <button 
                type="button" 
                className="btn-primary py-2 px-4 text-xs font-semibold"
                onClick={onButtonClick}
              >
                Change File
              </button>
            </div>
          </div>
        ) : (
          <div className="flex flex-col items-center space-y-3 pointer-events-none">
            <div className="p-4 bg-white/5 rounded-full text-text-muted transition-colors border border-white/5 group-hover:border-purple-500/20">
              <Upload size={32} className="stroke-[1.5] text-purple-400" />
            </div>
            <div>
              <p className="font-display font-semibold text-base text-white">{title}</p>
              <p className="text-xs text-text-muted mt-1">{subtitle}</p>
            </div>
          </div>
        )}
      </div>

      {error && (
        <div className="mt-4 flex items-center gap-2 text-pink-500 text-sm bg-pink-950/20 border border-pink-500/20 py-2 px-4 rounded-lg animate-bounce">
          <AlertCircle size={16} />
          <span>{error}</span>
        </div>
      )}
    </div>
  );
}
