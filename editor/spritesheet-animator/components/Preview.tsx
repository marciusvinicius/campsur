
import React, { useState, useEffect, useRef } from 'react';
import { ProjectState } from '../types';

interface PreviewProps {
  project: ProjectState;
}

const Preview: React.FC<PreviewProps> = ({ project }) => {
  const [frameIndex, setFrameIndex] = useState(0);
  const [fps, setFps] = useState(12);
  const [isPlaying, setIsPlaying] = useState(true);
  const timerRef = useRef<number | null>(null);

  const activeAnim = project.animations[project.activeAnimation] || [];

  useEffect(() => {
    if (!isPlaying || activeAnim.length === 0) {
      if (timerRef.current) window.clearInterval(timerRef.current);
      return;
    }

    timerRef.current = window.setInterval(() => {
      setFrameIndex(prev => (prev + 1) % activeAnim.length);
    }, 1000 / fps);

    return () => {
      if (timerRef.current) window.clearInterval(timerRef.current);
    };
  }, [isPlaying, activeAnim.length, fps]);

  // Reset frame index if animation changes
  useEffect(() => {
    setFrameIndex(0);
  }, [project.activeAnimation]);

  if (!project.imageUrl || activeAnim.length === 0) {
    return <span className="text-xs text-slate-600">No frames selected</span>;
  }

  const currentFrame = activeAnim[frameIndex];

  return (
    <div className="flex flex-col items-center gap-4 w-full h-full p-4">
      <div className="flex-1 flex items-center justify-center w-full overflow-hidden">
        <div 
          style={{
            width: `${currentFrame.width}px`,
            height: `${currentFrame.height}px`,
            backgroundImage: `url(${project.imageUrl})`,
            backgroundPosition: `-${currentFrame.x}px -${currentFrame.y}px`,
            backgroundRepeat: 'no-repeat',
            imageRendering: 'pixelated',
            transform: 'scale(1.5)',
          }}
        />
      </div>
      
      <div className="w-full flex items-center justify-between bg-slate-900/80 p-2 rounded-lg gap-2">
        <button 
          onClick={() => setIsPlaying(!isPlaying)}
          className="p-1 hover:text-indigo-400"
        >
          {isPlaying ? (
            <svg className="w-6 h-6" fill="currentColor" viewBox="0 0 20 20"><path fillRule="evenodd" d="M18 10a8 8 0 11-16 0 8 8 0 0116 0zM7 8a1 1 0 012 0v4a1 1 0 11-2 0V8zm5-1a1 1 0 00-1 1v4a1 1 0 102 0V8a1 1 0 00-1-1z" clipRule="evenodd" /></svg>
          ) : (
            <svg className="w-6 h-6" fill="currentColor" viewBox="0 0 20 20"><path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM9.555 7.168A1 1 0 008 8v4a1 1 0 001.555.832l3-2a1 1 0 000-1.664l-3-2z" clipRule="evenodd" /></svg>
          )}
        </button>
        
        <div className="flex items-center gap-2 flex-1 px-2">
          <input 
            type="range" 
            min="1" 
            max="60" 
            value={fps} 
            onChange={(e) => setFps(parseInt(e.target.value))}
            className="w-full h-1 bg-slate-700 rounded-lg appearance-none cursor-pointer accent-indigo-500"
          />
          <span className="text-[10px] font-mono text-slate-400 w-8 text-right">{fps}fps</span>
        </div>
      </div>
    </div>
  );
};

export default Preview;
