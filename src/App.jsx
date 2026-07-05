import React, { useState } from 'react';
import { Sliders, Scissors, RefreshCw, Shield, ChevronLeft, Github, Lock, Info, ServerCrash } from 'lucide-react';
import Compressor from './tools/Compressor';
import MergerSplitter from './tools/MergerSplitter';
import Converter from './tools/Converter';
import Protector from './tools/Protector';

export default function App() {
  const [activeTool, setActiveTool] = useState(null); // null means dashboard, or 'compressor', 'merge-split', 'converter', 'protector'

  const tools = [
    {
      id: 'compressor',
      title: 'PDF Shortener',
      description: 'Compress PDF size down to specific byte targets while preserving sharp font readability.',
      icon: <Sliders size={24} />,
      color: 'from-purple-500 to-indigo-500',
      shadow: 'rgba(168, 85, 247, 0.25)',
      component: <Compressor />
    },
    {
      id: 'merge-split',
      title: 'Split & Merge',
      description: 'Combine multiple document streams in order or partition specific page indices.',
      icon: <Scissors size={24} />,
      color: 'from-blue-500 to-cyan-500',
      shadow: 'rgba(6, 182, 212, 0.25)',
      component: <MergerSplitter />
    },
    {
      id: 'converter',
      title: 'Doc Converter',
      description: 'Rebuild PDF pages into editable Microsoft Word (.docx), PowerPoint (.pptx) templates, or images.',
      icon: <RefreshCw size={24} />,
      color: 'from-emerald-500 to-teal-500',
      shadow: 'rgba(16, 185, 129, 0.25)',
      component: <Converter />
    },
    {
      id: 'protector',
      title: 'Protect & Overlay',
      description: 'Encrypt documents with owner passwords and layer custom text watermarks.',
      icon: <Shield size={24} />,
      color: 'from-pink-500 to-rose-500',
      shadow: 'rgba(244, 63, 94, 0.25)',
      component: <Protector />
    }
  ];

  return (
    <div className="min-h-screen flex flex-col justify-between">
      
      {/* Dynamic Background Glows */}
      <div className="absolute top-0 left-1/4 w-96 h-96 bg-purple-500/10 rounded-full blur-3xl pointer-events-none" />
      <div className="absolute bottom-10 right-1/4 w-96 h-96 bg-cyan-500/5 rounded-full blur-3xl pointer-events-none" />

      {/* Header Container */}
      <header className="border-b border-white/5 bg-black/30 backdrop-blur-md sticky top-0 z-50">
        <div className="max-w-6xl mx-auto px-6 py-4 flex items-center justify-between">
          <div 
            onClick={() => setActiveTool(null)}
            className="flex items-center gap-2.5 cursor-pointer group"
          >
            <div className="w-8 h-8 rounded-lg bg-gradient-to-r from-purple-500 to-cyan-500 flex items-center justify-center text-white font-extrabold shadow-[0_0_15px_rgba(168,85,247,0.4)] group-hover:scale-105 transition-transform">
              P
            </div>
            <span className="font-display font-extrabold text-xl tracking-wider text-transparent bg-clip-text bg-gradient-to-r from-white via-slate-100 to-purple-400 group-hover:opacity-90 transition-opacity">
              pdfshooter
            </span>
          </div>

          <div className="flex items-center gap-4">
            <span className="hidden md:inline-flex items-center gap-1.5 text-xs text-emerald-400 bg-emerald-500/10 px-3 py-1 rounded-full border border-emerald-500/20 font-semibold shadow-[0_0_10px_rgba(16,185,129,0.05)]">
              <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
              100% Client-Side Sandbox
            </span>
            <a 
              href="https://github.com" 
              target="_blank" 
              rel="noopener noreferrer"
              className="text-text-muted hover:text-white transition-colors"
            >
              <Github size={20} />
            </a>
          </div>
        </div>
      </header>

      {/* Main Workspace */}
      <main className="flex-grow max-w-6xl w-full mx-auto px-6 py-8 relative z-10 flex flex-col justify-center">
        
        {/* If a tool is active, display the workspace toolbar and component */}
        {activeTool ? (
          <div className="flex flex-col space-y-6">
            <div className="flex items-center">
              <button
                type="button"
                onClick={() => setActiveTool(null)}
                className="btn-secondary py-2 px-3 text-xs flex items-center gap-1.5"
              >
                <ChevronLeft size={14} />
                Back to Dashboard
              </button>
            </div>
            <div className="glass-panel p-2 md:p-6 min-h-[500px] flex items-center justify-center">
              {tools.find(t => t.id === activeTool)?.component}
            </div>
          </div>
        ) : (
          // Dashboard Hub
          <div className="space-y-12 py-6">
            
            {/* Hero / Header text */}
            <div className="text-center space-y-4 max-w-2xl mx-auto">
              <h1 className="font-display font-extrabold text-4xl md:text-5xl tracking-tight leading-tight">
                Secure & Fast PDF <br/>
                <span className="gradient-text">File Operations</span>
              </h1>
              <p className="text-sm md:text-base text-text-muted leading-relaxed">
                pdfshooter executes all rendering, resizing, conversion, and locks <strong>directly in your browser sandbox</strong>. Zero bytes transfer to external servers, rendering sniffing and server DDoS issues impossible.
              </p>
            </div>

            {/* Security Indicator Panel */}
            <div className="glass-panel p-5 grid grid-cols-1 md:grid-cols-3 gap-6 max-w-4xl mx-auto border-purple-500/10">
              <div className="flex gap-3">
                <div className="p-2.5 bg-emerald-500/10 rounded-xl text-emerald-400 h-10 w-10 flex-shrink-0 flex items-center justify-center">
                  <Lock size={18} />
                </div>
                <div>
                  <h4 className="font-display font-bold text-sm text-white">Absolute Privacy</h4>
                  <p className="text-xs text-text-muted mt-1 leading-relaxed">Your files never upload to any database. Operations happen inside web workers.</p>
                </div>
              </div>
              <div className="flex gap-3">
                <div className="p-2.5 bg-cyan-500/10 rounded-xl text-cyan-400 h-10 w-10 flex-shrink-0 flex items-center justify-center">
                  <ServerCrash size={18} />
                </div>
                <div>
                  <h4 className="font-display font-bold text-sm text-white">DDoS Immune</h4>
                  <p className="text-xs text-text-muted mt-1 leading-relaxed">No backend processing servers. Static hostings scale infinitely against request spamming.</p>
                </div>
              </div>
              <div className="flex gap-3">
                <div className="p-2.5 bg-purple-500/10 rounded-xl text-purple-400 h-10 w-10 flex-shrink-0 flex items-center justify-center">
                  <Info size={18} />
                </div>
                <div>
                  <h4 className="font-display font-bold text-sm text-white">Resume-Ready Features</h4>
                  <p className="text-xs text-text-muted mt-1 leading-relaxed">Integrates HTML5 Canvases, custom JPEG compression matrices, and Office XML builders.</p>
                </div>
              </div>
            </div>

            {/* Tools Grid */}
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6 max-w-4xl mx-auto">
              {tools.map((tool) => (
                <div
                  key={tool.id}
                  onClick={() => setActiveTool(tool.id)}
                  className="glass-panel p-6 flex flex-col justify-between cursor-pointer group relative overflow-hidden"
                  style={{
                    '--panel-border-hover': tool.id === 'compressor' ? 'rgba(168,85,247,0.4)' : tool.id === 'merge-split' ? 'rgba(6,182,212,0.4)' : tool.id === 'converter' ? 'rgba(16,185,129,0.4)' : 'rgba(244,63,94,0.4)'
                  }}
                >
                  <div className="absolute top-0 right-0 w-24 h-24 bg-gradient-to-br from-white/5 to-transparent rounded-bl-full pointer-events-none transition-all group-hover:scale-110" />
                  
                  <div className="space-y-4">
                    <div className={`w-12 h-12 rounded-xl bg-gradient-to-r ${tool.color} flex items-center justify-center text-white shadow-[0_4px_12px_${tool.shadow}] group-hover:scale-105 transition-transform`}>
                      {tool.icon}
                    </div>
                    
                    <div>
                      <h3 className="font-display font-bold text-xl text-white group-hover:text-purple-300 transition-colors">
                        {tool.title}
                      </h3>
                      <p className="text-sm text-text-muted mt-2 leading-relaxed">
                        {tool.description}
                      </p>
                    </div>
                  </div>

                  <div className="mt-6 flex items-center text-xs font-semibold text-cyan-400 group-hover:translate-x-1 transition-transform">
                    Launch Workspace &rarr;
                  </div>
                </div>
              ))}
            </div>

          </div>
        )}
      </main>

      {/* Footer */}
      <footer className="border-t border-white/5 bg-black/60 backdrop-blur-sm py-6 mt-12 relative z-10 text-center">
        <div className="max-w-6xl mx-auto px-6 flex flex-col md:flex-row items-center justify-between gap-4 text-xs text-text-muted">
          <p>&copy; {new Date().getFullYear()} pdfshooter. Open-Source browser workspace.</p>
          <div className="flex gap-4">
            <span className="flex items-center gap-1">
              <span className="w-1.5 h-1.5 rounded-full bg-cyan-400" />
              100% Client-Side Sandbox
            </span>
            <span className="flex items-center gap-1">
              <span className="w-1.5 h-1.5 rounded-full bg-purple-400" />
              Local Storage Caching
            </span>
          </div>
        </div>
      </footer>

    </div>
  );
}
