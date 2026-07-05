import React, { useState } from 'react';
import { Layers, Scissors, Plus, Trash2, ArrowUp, ArrowDown, FileText, Download, CheckCircle, AlertCircle } from 'lucide-react';
import { PDFDocument } from 'pdf-lib';
import DropZone from '../components/DropZone';

export default function MergerSplitter() {
  const [activeTab, setActiveTab] = useState('merge'); // 'merge' or 'split'
  
  // Merger State
  const [mergeFiles, setMergeFiles] = useState([]);
  const [merging, setMerging] = useState(false);
  const [mergedBlob, setMergedBlob] = useState(null);

  // Splitter State
  const [splitFile, setSplitFile] = useState(null);
  const [splitMethod, setSplitMethod] = useState('range'); // 'range' or 'pages'
  const [splitRangeStart, setSplitRangeStart] = useState('1');
  const [splitRangeEnd, setSplitRangeEnd] = useState('1');
  const [customPagesInput, setCustomPagesInput] = useState('1, 3');
  const [splitting, setSplitting] = useState(false);
  const [splitBlob, setSplitBlob] = useState(null);
  const [totalPagesCount, setTotalPagesCount] = useState(0);
  const [splitError, setSplitError] = useState(null);

  // --- MERGER LOGIC ---
  const handleMergeFilesSelected = (selectedFiles) => {
    if (!selectedFiles) return;
    const fileList = Array.isArray(selectedFiles) ? selectedFiles : [selectedFiles];
    setMergeFiles((prev) => [...prev, ...fileList]);
    setMergedBlob(null);
  };

  const handleRemoveMergeFile = (index) => {
    setMergeFiles((prev) => prev.filter((_, i) => i !== index));
    setMergedBlob(null);
  };

  const handleMoveFile = (index, direction) => {
    if (direction === 'up' && index === 0) return;
    if (direction === 'down' && index === mergeFiles.length - 1) return;
    
    const targetIndex = direction === 'up' ? index - 1 : index + 1;
    const updated = [...mergeFiles];
    const temp = updated[index];
    updated[index] = updated[targetIndex];
    updated[targetIndex] = temp;
    setMergeFiles(updated);
    setMergedBlob(null);
  };

  const handleMerge = async () => {
    if (mergeFiles.length < 2) return;
    setMerging(true);
    setMergedBlob(null);

    try {
      const mergedPdf = await PDFDocument.create();
      for (const file of mergeFiles) {
        const fileBytes = await file.arrayBuffer();
        const srcDoc = await PDFDocument.load(fileBytes);
        const copiedPages = await mergedPdf.copyPages(srcDoc, srcDoc.getPageIndices());
        copiedPages.forEach((page) => mergedPdf.addPage(page));
      }
      const pdfBytes = await mergedPdf.save();
      const blob = new Blob([pdfBytes], { type: 'application/pdf' });
      setMergedBlob(blob);
    } catch (err) {
      console.error('Error merging PDFs:', err);
      alert('Failed to merge PDFs. Please make sure the files are not corrupted or password-protected.');
    } finally {
      setMerging(false);
    }
  };

  const downloadMerged = () => {
    if (!mergedBlob) return;
    const url = URL.createObjectURL(mergedBlob);
    const link = document.createElement('a');
    link.href = url;
    link.download = "merged_document.pdf";
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  // --- SPLITTER LOGIC ---
  const handleSplitFileSelected = async (file) => {
    setSplitFile(file);
    setSplitBlob(null);
    setSplitError(null);
    if (!file) return;

    try {
      const fileBytes = await file.arrayBuffer();
      const srcDoc = await PDFDocument.load(fileBytes);
      const pageCount = srcDoc.getPageCount();
      setTotalPagesCount(pageCount);
      setSplitRangeEnd(pageCount.toString());
    } catch (err) {
      console.error('Error parsing pages count:', err);
      setSplitError('Could not read PDF structure.');
    }
  };

  const parsePageIndices = () => {
    const indices = [];
    if (splitMethod === 'range') {
      const start = parseInt(splitRangeStart);
      const end = parseInt(splitRangeEnd);
      if (isNaN(start) || isNaN(end) || start < 1 || end < start || end > totalPagesCount) {
        throw new Error(`Invalid page range. Must be between 1 and ${totalPagesCount}.`);
      }
      for (let i = start - 1; i < end; i++) {
        indices.push(i);
      }
    } else {
      // Custom pages list (e.g. "1, 3, 5-7")
      const segments = customPagesInput.split(',');
      for (let segment of segments) {
        segment = segment.trim();
        if (segment.includes('-')) {
          const [sStr, eStr] = segment.split('-');
          const s = parseInt(sStr);
          const e = parseInt(eStr);
          if (isNaN(s) || isNaN(e) || s < 1 || e < s || e > totalPagesCount) {
            throw new Error(`Invalid custom range "${segment}". Pages must be between 1 and ${totalPagesCount}.`);
          }
          for (let i = s - 1; i < e; i++) {
            indices.push(i);
          }
        } else {
          const p = parseInt(segment);
          if (isNaN(p) || p < 1 || p > totalPagesCount) {
            throw new Error(`Invalid page number "${segment}". Pages must be between 1 and ${totalPagesCount}.`);
          }
          indices.push(p - 1);
        }
      }
    }
    // Deduplicate indices and sort
    return Array.from(new Set(indices)).sort((a, b) => a - b);
  };

  const handleSplit = async () => {
    if (!splitFile) return;
    setSplitting(true);
    setSplitBlob(null);
    setSplitError(null);

    try {
      const targetIndices = parsePageIndices();
      if (targetIndices.length === 0) {
        throw new Error('No pages selected for extraction.');
      }

      const fileBytes = await splitFile.arrayBuffer();
      const srcDoc = await PDFDocument.load(fileBytes);
      const splitPdf = await PDFDocument.create();

      const copiedPages = await splitPdf.copyPages(srcDoc, targetIndices);
      copiedPages.forEach((page) => splitPdf.addPage(page));

      const pdfBytes = await splitPdf.save();
      const blob = new Blob([pdfBytes], { type: 'application/pdf' });
      setSplitBlob(blob);
    } catch (err) {
      console.error('Error splitting PDF:', err);
      setSplitError(err.message || 'Failed to split PDF. Validate page entries.');
    } finally {
      setSplitting(false);
    }
  };

  const downloadSplit = () => {
    if (!splitBlob) return;
    const url = URL.createObjectURL(splitBlob);
    const link = document.createElement('a');
    link.href = url;
    const originalName = splitFile.name.replace(/\.[^/.]+$/, "");
    link.download = `${originalName}_extracted.pdf`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  return (
    <div className="w-full flex flex-col items-center py-6 px-4 animate-fade-in">
      <div className="text-center max-w-xl mb-8">
        <h2 className="font-display font-extrabold text-3xl mb-2 text-transparent bg-clip-text bg-gradient-to-r from-purple-400 via-indigo-200 to-cyan-400">
          Split & Merge PDFs
        </h2>
        <p className="text-sm text-text-muted">
          Reconstruct your documents. Combine multiple files into one file or extract specific ranges and pages instantly in your browser.
        </p>
      </div>

      {/* Tabs */}
      <div className="flex border border-white/5 bg-white/5 p-1.5 rounded-xl mb-8 w-full max-w-md">
        <button
          type="button"
          onClick={() => setActiveTab('merge')}
          className={`flex-1 flex items-center justify-center gap-2 py-2.5 rounded-lg text-sm font-semibold transition-all ${
            activeTab === 'merge'
              ? 'bg-purple-600/35 border border-purple-500/30 text-white shadow-lg'
              : 'text-text-muted hover:text-white'
          }`}
        >
          <Layers size={16} />
          Merge Documents
        </button>
        <button
          type="button"
          onClick={() => setActiveTab('split')}
          className={`flex-1 flex items-center justify-center gap-2 py-2.5 rounded-lg text-sm font-semibold transition-all ${
            activeTab === 'split'
              ? 'bg-purple-600/35 border border-purple-500/30 text-white shadow-lg'
              : 'text-text-muted hover:text-white'
          }`}
        >
          <Scissors size={16} />
          Extract / Split Pages
        </button>
      </div>

      {/* Tab Contents: MERGE */}
      {activeTab === 'merge' && (
        <div className="w-full max-w-2xl space-y-6">
          <DropZone 
            onFilesSelected={handleMergeFilesSelected} 
            multiple={true} 
            title="Upload multiple PDFs to merge"
            subtitle="Drag & drop or click to add files"
          />

          {mergeFiles.length > 0 && (
            <div className="glass-panel p-6 space-y-4">
              <h3 className="font-display font-semibold text-lg text-white flex justify-between items-center">
                <span>Documents for Merging ({mergeFiles.length})</span>
                <button
                  type="button"
                  onClick={() => { setMergeFiles([]); setMergedBlob(null); }}
                  className="text-xs text-pink-500 hover:text-pink-400 flex items-center gap-1"
                >
                  <Trash2 size={14} />
                  Clear All
                </button>
              </h3>

              {/* Files Queue List */}
              <div className="space-y-2 max-h-[300px] overflow-y-auto pr-1">
                {mergeFiles.map((file, idx) => (
                  <div 
                    key={`${file.name}-${idx}`} 
                    className="flex items-center justify-between p-3 bg-white/5 border border-white/5 rounded-xl text-sm"
                  >
                    <div className="flex items-center gap-3 overflow-hidden mr-4">
                      <div className="p-2 bg-purple-500/10 rounded-lg text-purple-400 flex-shrink-0">
                        <FileText size={16} />
                      </div>
                      <span className="font-semibold truncate text-white">{file.name}</span>
                      <span className="text-xs text-text-muted">({(file.size / (1024 * 1024)).toFixed(2)} MB)</span>
                    </div>
                    <div className="flex items-center gap-1.5 flex-shrink-0">
                      <button
                        type="button"
                        disabled={idx === 0}
                        onClick={() => handleMoveFile(idx, 'up')}
                        className="p-1.5 hover:bg-white/10 rounded-lg text-text-muted disabled:opacity-30"
                      >
                        <ArrowUp size={14} />
                      </button>
                      <button
                        type="button"
                        disabled={idx === mergeFiles.length - 1}
                        onClick={() => handleMoveFile(idx, 'down')}
                        className="p-1.5 hover:bg-white/10 rounded-lg text-text-muted disabled:opacity-30"
                      >
                        <ArrowDown size={14} />
                      </button>
                      <button
                        type="button"
                        onClick={() => handleRemoveMergeFile(idx)}
                        className="p-1.5 hover:bg-pink-500/20 text-pink-400 hover:text-pink-300 rounded-lg"
                      >
                        <Trash2 size={14} />
                      </button>
                    </div>
                  </div>
                ))}
              </div>

              {/* Action and Output block */}
              <div className="pt-4 border-t border-white/5 flex flex-col gap-4">
                {!merging && !mergedBlob && (
                  <button
                    type="button"
                    onClick={handleMerge}
                    disabled={mergeFiles.length < 2}
                    className="btn-primary justify-center w-full"
                  >
                    <Layers size={18} />
                    Shoot & Merge Files
                  </button>
                )}

                {merging && (
                  <div className="flex items-center justify-center gap-2 py-3 text-cyan-400 text-sm">
                    <span className="spinner border-2 border-cyan-400 border-t-transparent w-4 h-4 rounded-full" />
                    Consolidating pages into memory buffer...
                  </div>
                )}

                {mergedBlob && (
                  <div className="p-4 bg-emerald-950/20 border border-emerald-500/20 rounded-xl space-y-3 animate-fade-in">
                    <div className="flex items-center gap-2 text-emerald-400 font-display font-semibold">
                      <CheckCircle size={18} />
                      PDFs Merged Successfully!
                    </div>
                    <button
                      type="button"
                      onClick={downloadMerged}
                      className="btn-primary w-full justify-center"
                    >
                      <Download size={18} />
                      Download Combined PDF
                    </button>
                  </div>
                )}
              </div>
            </div>
          )}
        </div>
      )}

      {/* Tab Contents: SPLIT */}
      {activeTab === 'split' && (
        <div className="w-full max-w-2xl space-y-6">
          <DropZone 
            onFilesSelected={handleSplitFileSelected} 
            title="Upload single PDF to extract pages"
            subtitle="Drag & drop or click to add file"
          />

          {splitFile && (
            <div className="glass-panel p-6 space-y-5">
              <div className="flex items-center justify-between border-b border-white/5 pb-3">
                <div className="flex items-center gap-3">
                  <div className="p-2.5 bg-cyan-500/10 rounded-lg text-cyan-400">
                    <FileText size={18} />
                  </div>
                  <div>
                    <h3 className="font-display font-semibold text-white">{splitFile.name}</h3>
                    <p className="text-xs text-text-muted mt-0.5">Total Pages: {totalPagesCount}</p>
                  </div>
                </div>
              </div>

              {/* Extraction method config */}
              <div className="space-y-4">
                <div className="flex gap-4">
                  <label className="flex items-center gap-2 text-xs font-semibold cursor-pointer text-text-muted hover:text-white">
                    <input
                      type="radio"
                      name="splitMethod"
                      value="range"
                      checked={splitMethod === 'range'}
                      onChange={() => { setSplitMethod('range'); setSplitBlob(null); }}
                      className="accent-purple-500"
                    />
                    Range Extraction
                  </label>
                  <label className="flex items-center gap-2 text-xs font-semibold cursor-pointer text-text-muted hover:text-white">
                    <input
                      type="radio"
                      name="splitMethod"
                      value="pages"
                      checked={splitMethod === 'pages'}
                      onChange={() => { setSplitMethod('pages'); setSplitBlob(null); }}
                      className="accent-purple-500"
                    />
                    Select Specific Pages
                  </label>
                </div>

                {splitMethod === 'range' ? (
                  <div className="grid grid-cols-2 gap-4 animate-fade-in">
                    <div className="flex flex-col space-y-1.5">
                      <span className="text-xs text-text-muted font-medium">Start Page</span>
                      <input
                        type="number"
                        min="1"
                        max={totalPagesCount}
                        value={splitRangeStart}
                        onChange={(e) => { setSplitRangeStart(e.target.value); setSplitBlob(null); }}
                        className="bg-white/5 border border-white/10 rounded-lg p-2.5 text-sm text-white focus:outline-none focus:border-purple-500"
                      />
                    </div>
                    <div className="flex flex-col space-y-1.5">
                      <span className="text-xs text-text-muted font-medium">End Page</span>
                      <input
                        type="number"
                        min="1"
                        max={totalPagesCount}
                        value={splitRangeEnd}
                        onChange={(e) => { setSplitRangeEnd(e.target.value); setSplitBlob(null); }}
                        className="bg-white/5 border border-white/10 rounded-lg p-2.5 text-sm text-white focus:outline-none focus:border-purple-500"
                      />
                    </div>
                  </div>
                ) : (
                  <div className="flex flex-col space-y-1.5 animate-fade-in">
                    <span className="text-xs text-text-muted font-medium">Page indices (comma-separated, e.g. "1, 3, 5-7")</span>
                    <input
                      type="text"
                      placeholder="e.g. 1, 3, 5"
                      value={customPagesInput}
                      onChange={(e) => { setCustomPagesInput(e.target.value); setSplitBlob(null); }}
                      className="bg-white/5 border border-white/10 rounded-lg p-2.5 text-sm text-white focus:outline-none focus:border-purple-500 font-mono"
                    />
                  </div>
                )}
              </div>

              {/* Submit & Status & Output */}
              <div className="pt-4 border-t border-white/5 flex flex-col gap-4">
                {splitError && (
                  <div className="flex items-center gap-2 text-pink-500 text-xs bg-pink-950/20 border border-pink-500/20 py-2 px-4 rounded-lg">
                    <AlertCircle size={14} />
                    <span>{splitError}</span>
                  </div>
                )}

                {!splitting && !splitBlob && (
                  <button
                    type="button"
                    onClick={handleSplit}
                    className="btn-primary justify-center w-full"
                  >
                    <Scissors size={18} />
                    Shoot & Extract Pages
                  </button>
                )}

                {splitting && (
                  <div className="flex items-center justify-center gap-2 py-3 text-cyan-400 text-sm">
                    <span className="spinner border-2 border-cyan-400 border-t-transparent w-4 h-4 rounded-full" />
                    Extracting requested indexes...
                  </div>
                )}

                {splitBlob && (
                  <div className="p-4 bg-emerald-950/20 border border-emerald-500/20 rounded-xl space-y-3 animate-fade-in">
                    <div className="flex items-center gap-2 text-emerald-400 font-display font-semibold">
                      <CheckCircle size={18} />
                      Pages Extracted Successfully!
                    </div>
                    <button
                      type="button"
                      onClick={downloadSplit}
                      className="btn-primary w-full justify-center"
                    >
                      <Download size={18} />
                      Download Split PDF
                    </button>
                  </div>
                )}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
