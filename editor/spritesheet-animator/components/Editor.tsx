
import React, { useRef, useEffect, useState, useCallback } from 'react';
import { ProjectState, Frame } from '../types';

interface EditorProps {
  project: ProjectState;
  onAddFrame: (frame: Frame) => void;
  onUpdateFrameSize: (w: number, h: number) => void;
}

const Editor: React.FC<EditorProps> = ({ project, onAddFrame, onUpdateFrameSize }) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const [zoom, setZoom] = useState(1);
  const [mousePos, setMousePos] = useState({ x: 0, y: 0 });
  const [isResizing, setIsResizing] = useState(false);
  const [dragStart, setDragStart] = useState({ x: 0, y: 0 });
  const [mode, setMode] = useState<'SELECT' | 'RESIZE'>('SELECT');
  const [showGrid, setShowGrid] = useState(true);

  const getCanvasCoords = useCallback((clientX: number, clientY: number) => {
    if (!containerRef.current) return { x: 0, y: 0 };
    const rect = containerRef.current.getBoundingClientRect();
    return {
      x: (clientX - rect.left) / zoom,
      y: (clientY - rect.top) / zoom
    };
  }, [zoom]);

  const handleMouseDown = (e: React.MouseEvent) => {
    const coords = getCanvasCoords(e.clientX, e.clientY);
    if (mode === 'RESIZE') {
      setIsResizing(true);
      setDragStart(coords);
    }
  };

  const handleMouseMove = (e: React.MouseEvent) => {
    const coords = getCanvasCoords(e.clientX, e.clientY);
    
    if (isResizing) {
      const width = Math.max(1, Math.round(coords.x - dragStart.x));
      const height = Math.max(1, Math.round(coords.y - dragStart.y));
      onUpdateFrameSize(width, height);
    } else {
      // Snap to grid
      const w = project.frameWidth || 1;
      const h = project.frameHeight || 1;
      const x = Math.floor(coords.x / w) * w;
      const y = Math.floor(coords.y / h) * h;
      setMousePos({ x, y });
    }
  };

  const handleMouseUp = () => {
    if (isResizing) {
      setIsResizing(false);
      setMode('SELECT');
    }
  };

  const triggerAddFrame = useCallback(() => {
    if (mode === 'RESIZE' || isResizing) return;
    if (!project.activeAnimation) return;
    
    // Constraint: don't add frames outside the image if possible
    onAddFrame({
      x: mousePos.x,
      y: mousePos.y,
      width: project.frameWidth,
      height: project.frameHeight
    });
  }, [mode, isResizing, project.activeAnimation, project.frameWidth, project.frameHeight, mousePos, onAddFrame]);

  const handleCanvasClick = (e: React.MouseEvent) => {
    triggerAddFrame();
  };

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      const target = e.target as HTMLElement;
      if (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA') return;

      const key = e.key.toLowerCase();
      switch (key) {
        case 'a':
          e.preventDefault();
          triggerAddFrame();
          break;
        case 'p':
          setMode('SELECT');
          break;
        case 'r':
          setMode('RESIZE');
          break;
        case 'g':
          setShowGrid(prev => !prev);
          break;
        default:
          break;
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [triggerAddFrame]);

  if (!project.imageUrl) {
    return (
      <div className="flex flex-col items-center justify-center h-full text-slate-500 p-8">
        <div className="w-20 h-20 bg-slate-800 rounded-full flex items-center justify-center mb-6">
          <svg className="w-10 h-10 opacity-40" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" />
          </svg>
        </div>
        <p className="text-xl font-bold text-slate-300">Sprite Sheet Required</p>
        <p className="text-sm mt-2 opacity-60 text-center max-w-xs leading-relaxed">
          Load a sprite sheet to automatically divide it into a grid and start mapping your animations.
        </p>
      </div>
    );
  }

  const activeFrames = project.animations[project.activeAnimation] || [];

  return (
    <div className="min-h-full min-w-full checkered-bg flex items-center justify-center p-20 select-none overflow-auto">
      <div 
        ref={containerRef}
        className={`relative bg-black/40 shadow-2xl transition-transform ring-1 ring-white/10 ${mode === 'RESIZE' ? 'cursor-nwse-resize' : 'cursor-pointer'}`}
        style={{ 
          transform: `scale(${zoom})`, 
          transformOrigin: 'center center',
          width: project.imageWidth,
          height: project.imageHeight
        }}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
        onClick={handleCanvasClick}
      >
        <img 
          src={project.imageUrl} 
          alt="Sprite Sheet" 
          className="block max-w-none pointer-events-none"
          style={{ imageRendering: 'pixelated' }}
        />
        
        {/* Grid Overlay strictly clipped to image */}
        {showGrid && project.frameWidth > 0 && project.frameHeight > 0 && (
          <div 
            className="absolute inset-0 pointer-events-none overflow-hidden"
            style={{
              backgroundImage: `linear-gradient(to right, rgba(71, 85, 105, 0.4) 1px, transparent 1px), linear-gradient(to bottom, rgba(71, 85, 105, 0.4) 1px, transparent 1px)`,
              backgroundSize: `${project.frameWidth}px ${project.frameHeight}px`
            }}
          />
        )}
        
        {/* Resize Action Indicator */}
        {isResizing && (
          <div 
            className="absolute border-2 border-dashed border-red-500 bg-red-500/10 pointer-events-none z-30"
            style={{
              left: dragStart.x,
              top: dragStart.y,
              width: project.frameWidth,
              height: project.frameHeight
            }}
          />
        )}

        {/* Selected Cell Highlight */}
        {!isResizing && mode === 'SELECT' && (
          <div 
            className="absolute border-2 border-yellow-400 bg-yellow-400/30 pointer-events-none z-20"
            style={{
              left: mousePos.x,
              top: mousePos.y,
              width: project.frameWidth,
              height: project.frameHeight,
              boxShadow: 'inset 0 0 10px rgba(250, 204, 21, 0.5), 0 0 20px rgba(250, 204, 21, 0.3)'
            }}
          >
             <div className="absolute top-0 left-0 bg-yellow-400 text-slate-900 text-[8px] font-bold px-1 py-0.5 leading-none shadow-md">
                {Math.floor(mousePos.x / project.frameWidth)},{Math.floor(mousePos.y / project.frameHeight)}
             </div>
          </div>
        )}

        {/* Registered Animation Frames */}
        {activeFrames.map((frame, idx) => (
          <div 
            key={`${idx}-${frame.x}-${frame.y}`}
            className="absolute border border-indigo-400 bg-indigo-500/20 pointer-events-none z-10"
            style={{
              left: frame.x,
              top: frame.y,
              width: frame.width,
              height: frame.height
            }}
          >
            <span className="absolute -top-6 left-0 bg-indigo-600 text-white text-[10px] px-1.5 py-0.5 rounded font-bold shadow-lg border border-indigo-400 whitespace-nowrap">
              FRAME {idx}
            </span>
          </div>
        ))}
      </div>

      {/* Floating Toolbar */}
      <div className="fixed bottom-6 left-1/2 -translate-x-1/2 flex items-center gap-4 bg-slate-800/90 backdrop-blur-md border border-slate-700/50 p-2.5 rounded-2xl shadow-[0_20px_50px_rgba(0,0,0,0.5)] z-20">
        <div className="flex bg-slate-900/50 p-1 rounded-xl border border-slate-700/50">
           <button 
             onClick={() => setMode('SELECT')}
             title="Pick Grid Cell (P)"
             className={`px-5 py-2 rounded-lg text-sm font-bold transition-all ${mode === 'SELECT' ? 'bg-indigo-600 text-white shadow-lg scale-105' : 'text-slate-500 hover:text-slate-300'}`}
           >
             Grid Select (P)
           </button>
           <button 
             onClick={() => setMode('RESIZE')}
             title="Resize Grid (R)"
             className={`px-5 py-2 rounded-lg text-sm font-bold transition-all ${mode === 'RESIZE' ? 'bg-indigo-600 text-white shadow-lg scale-105' : 'text-slate-500 hover:text-slate-300'}`}
           >
             Set Grid Size (R)
           </button>
        </div>

        <div className="h-8 w-px bg-slate-700/50" />

        <button 
          onClick={() => setShowGrid(!showGrid)}
          className={`px-4 py-2 rounded-lg text-xs font-black tracking-tighter border transition-all ${showGrid ? 'bg-indigo-600/10 border-indigo-500/50 text-indigo-400 shadow-inner' : 'bg-slate-700/50 border-slate-600/50 text-slate-500'}`}
          title="Toggle Grid (G)"
        >
          {showGrid ? 'GRID ON' : 'GRID OFF'}
        </button>

        <div className="h-8 w-px bg-slate-700/50" />

        <div className="flex items-center gap-2">
          <button onClick={() => setZoom(z => Math.max(0.2, z - 0.2))} className="hover:text-indigo-400 p-1.5 transition-colors bg-slate-900/50 rounded-lg border border-slate-700/50">
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2.5} d="M20 12H4" /></svg>
          </button>
          <span className="text-[11px] font-mono min-w-[3.5rem] text-center text-slate-400 bg-slate-900/50 py-1 rounded-lg border border-slate-700/50 font-bold">{Math.round(zoom * 100)}%</span>
          <button onClick={() => setZoom(z => Math.min(5, z + 0.2))} className="hover:text-indigo-400 p-1.5 transition-colors bg-slate-900/50 rounded-lg border border-slate-700/50">
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2.5} d="M12 4v16m8-8H4" /></svg>
          </button>
        </div>
        
        <div className="h-8 w-px bg-slate-700/50" />
        
        <div className="px-3 text-[10px] text-slate-400 font-bold leading-tight select-none">
          <div className="flex items-center justify-between gap-6 mb-1">
            <span className="opacity-50">CELL:</span>
            <span className="text-indigo-400 tabular-nums">{project.frameWidth}x{project.frameHeight}</span>
          </div>
          <div className="flex items-center justify-between gap-6">
             <span className="opacity-50">COORD:</span>
             <span className="text-white tabular-nums">{Math.floor(mousePos.x / (project.frameWidth || 1))},{Math.floor(mousePos.y / (project.frameHeight || 1))}</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Editor;
