
import React, { useState, useEffect, useRef } from 'react';
import { ProjectState, Frame } from '../types';
import Preview from './Preview';

interface SidebarProps {
  project: ProjectState;
  onUpdateSize: (w: number, h: number) => void;
  onUpdateTexturePath: (path: string) => void;
  onAddAnim: (name: string) => void;
  onRenameAnim: (oldName: string, newName: string) => void;
  onDeleteAnim: (name: string) => void;
  onSelectAnim: (name: string) => void;
  onRemoveFrame: (animName: string, index: number) => void;
}

const Sidebar: React.FC<SidebarProps> = ({ 
  project, 
  onUpdateSize,
  onUpdateTexturePath,
  onAddAnim, 
  onRenameAnim,
  onDeleteAnim, 
  onSelectAnim,
  onRemoveFrame
}) => {
  const [newAnimName, setNewAnimName] = useState('');
  const [editingName, setEditingName] = useState<string | null>(null);
  const [editValue, setEditValue] = useState('');
  const editInputRef = useRef<HTMLInputElement>(null);

  // Local state for grid division inputs
  const cols = project.imageWidth > 0 ? Math.floor(project.imageWidth / (project.frameWidth || 1)) : 0;
  const rows = project.imageHeight > 0 ? Math.floor(project.imageHeight / (project.frameHeight || 1)) : 0;

  useEffect(() => {
    if (editingName && editInputRef.current) {
      editInputRef.current.focus();
      editInputRef.current.select();
    }
  }, [editingName]);

  const handleAdd = (e: React.FormEvent) => {
    e.preventDefault();
    if (newAnimName) {
      onAddAnim(newAnimName);
      setNewAnimName('');
    }
  };

  const handleRenameCommit = () => {
    if (editingName && editValue && editValue !== editingName) {
      onRenameAnim(editingName, editValue);
    }
    setEditingName(null);
  };

  const handleColChange = (val: number) => {
    if (val > 0 && project.imageWidth > 0) {
      onUpdateSize(Math.floor(project.imageWidth / val), project.frameHeight);
    }
  };

  const handleRowChange = (val: number) => {
    if (val > 0 && project.imageHeight > 0) {
      onUpdateSize(project.frameWidth, Math.floor(project.imageHeight / val));
    }
  };

  return (
    <div className="flex flex-col bg-slate-900">
      {/* Project Configuration */}
      <section className="p-4 border-b border-slate-800 space-y-5 bg-slate-900 sticky top-0 z-10 shadow-md">
        <div>
          <h3 className="text-xs font-bold uppercase tracking-wider text-slate-500 mb-3 flex items-center gap-2">
            <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z" /><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" /></svg>
            Project Setup
          </h3>
          <label className="block text-[10px] text-slate-400 mb-1 uppercase font-bold">Texture Full Path</label>
          <input 
            type="text" 
            value={project.texturePath}
            onChange={(e) => onUpdateTexturePath(e.target.value)}
            className="w-full bg-slate-800 border border-slate-700 rounded px-2 py-1.5 text-sm outline-none focus:border-indigo-500 font-mono transition-colors"
            placeholder="e.g. assets/textures/hero.png"
          />
          {project.imageWidth > 0 && (
            <p className="text-[10px] text-slate-500 mt-1.5 font-mono">DIMENSIONS: {project.imageWidth}x{project.imageHeight}</p>
          )}
        </div>

        <div>
          <h3 className="text-xs font-bold uppercase tracking-wider text-slate-500 mb-3 flex items-center gap-2">
            <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 6h16M4 12h16m-7 6h7" /></svg>
            Grid Partitioning
          </h3>
          <div className="grid grid-cols-2 gap-3 mb-4">
            <div>
              <label className="block text-[10px] text-slate-400 mb-1 uppercase font-bold">Columns</label>
              <input 
                type="number" 
                value={cols}
                onChange={(e) => handleColChange(parseInt(e.target.value) || 1)}
                className="w-full bg-slate-800 border border-slate-700 rounded px-2 py-1.5 text-sm outline-none focus:border-indigo-500 transition-colors"
              />
            </div>
            <div>
              <label className="block text-[10px] text-slate-400 mb-1 uppercase font-bold">Rows</label>
              <input 
                type="number" 
                value={rows}
                onChange={(e) => handleRowChange(parseInt(e.target.value) || 1)}
                className="w-full bg-slate-800 border border-slate-700 rounded px-2 py-1.5 text-sm outline-none focus:border-indigo-500 transition-colors"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-[10px] text-slate-400 mb-1 uppercase font-bold">Width (px)</label>
              <input 
                type="number" 
                value={project.frameWidth}
                onChange={(e) => onUpdateSize(parseInt(e.target.value) || 0, project.frameHeight)}
                className="w-full bg-slate-800 border border-slate-700 rounded px-2 py-1.5 text-sm outline-none focus:border-indigo-500 transition-colors"
              />
            </div>
            <div>
              <label className="block text-[10px] text-slate-400 mb-1 uppercase font-bold">Height (px)</label>
              <input 
                type="number" 
                value={project.frameHeight}
                onChange={(e) => onUpdateSize(project.frameWidth, parseInt(e.target.value) || 0)}
                className="w-full bg-slate-800 border border-slate-700 rounded px-2 py-1.5 text-sm outline-none focus:border-indigo-500 transition-colors"
              />
            </div>
          </div>
        </div>
      </section>

      {/* Animation Manager */}
      <section className="p-4 bg-slate-900 border-b border-slate-800">
        <div className="flex items-center justify-between mb-4">
          <h3 className="text-xs font-bold uppercase tracking-wider text-slate-500">Animation Sets</h3>
        </div>
        
        <form onSubmit={handleAdd} className="mb-6 flex gap-2">
          <input 
            type="text" 
            placeholder="New animation name..."
            value={newAnimName}
            onChange={(e) => setNewAnimName(e.target.value)}
            className="flex-1 bg-slate-800 border border-slate-700 rounded px-3 py-2 text-sm outline-none focus:border-indigo-500 shadow-inner"
          />
          <button type="submit" className="px-3 bg-indigo-600 hover:bg-indigo-500 rounded-lg transition-all shadow-lg active:scale-95">
            <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2.5} d="M12 4v16m8-8H4" /></svg>
          </button>
        </form>

        <div className="space-y-3">
          {Object.keys(project.animations).length === 0 && (
            <div className="text-center py-10 px-4 border-2 border-dashed border-slate-800 rounded-xl">
              <p className="text-xs text-slate-600 italic">No animations created yet.</p>
            </div>
          )}
          {Object.keys(project.animations).map((name) => (
            <div 
              key={name}
              className={`group rounded-xl border p-4 transition-all cursor-pointer ${
                project.activeAnimation === name 
                ? 'bg-indigo-600/10 border-indigo-500 shadow-[0_0_20px_rgba(79,70,229,0.1)]' 
                : 'bg-slate-800/40 border-slate-800 hover:border-slate-700'
              }`}
              onClick={() => onSelectAnim(name)}
            >
              <div className="flex items-center justify-between mb-3 min-h-[2rem]">
                {editingName === name ? (
                  <input
                    ref={editInputRef}
                    className="flex-1 bg-slate-950 border border-indigo-500 rounded px-2 py-0.5 text-sm outline-none text-indigo-100"
                    value={editValue}
                    onChange={(e) => setEditValue(e.target.value)}
                    onBlur={handleRenameCommit}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter') handleRenameCommit();
                      if (e.key === 'Escape') setEditingName(null);
                    }}
                    onClick={(e) => e.stopPropagation()}
                  />
                ) : (
                  <div className="flex items-center gap-2 flex-1 min-w-0">
                    <span className={`text-sm font-bold truncate ${project.activeAnimation === name ? 'text-indigo-300' : 'text-slate-300'}`}>
                      {name}
                    </span>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setEditingName(name);
                        setEditValue(name);
                      }}
                      className="opacity-0 group-hover:opacity-100 p-1 hover:text-indigo-400 text-slate-500 transition-opacity"
                      title="Rename"
                    >
                      <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z" />
                      </svg>
                    </button>
                  </div>
                )}
                
                <div className="flex items-center gap-2">
                  <span className="text-[10px] bg-slate-900/50 text-slate-400 px-2 py-0.5 rounded-full font-bold border border-slate-700">
                    {project.animations[name].length}
                  </span>
                  <button 
                    onClick={(e) => { e.stopPropagation(); onDeleteAnim(name); }}
                    className="opacity-0 group-hover:opacity-100 hover:text-red-400 transition-opacity p-1.5 bg-slate-900/50 rounded-lg"
                  >
                    <svg className="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" /></svg>
                  </button>
                </div>
              </div>

              {/* Frame Strip Preview */}
              <div className="flex gap-2 overflow-x-auto py-1 scrollbar-hide">
                {project.animations[name].length === 0 && (
                  <div className="text-[9px] text-slate-600 uppercase font-bold tracking-widest py-2">No frames added</div>
                )}
                {project.animations[name].map((_, idx) => (
                  <div 
                    key={idx}
                    className="flex-shrink-0 w-10 h-10 bg-slate-900 rounded-lg text-[10px] flex items-center justify-center relative group/frame shadow-inner border border-slate-700 font-bold"
                    onClick={(e) => {
                      e.stopPropagation();
                      onRemoveFrame(name, idx);
                    }}
                  >
                    {idx}
                    <div className="absolute inset-0 bg-red-500/90 rounded-lg opacity-0 group-hover/frame:opacity-100 flex items-center justify-center transition-opacity">
                      <svg className="w-5 h-5 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={3.5} d="M6 18L18 6M6 6l12 12" /></svg>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          ))}
        </div>
      </section>

      {/* Animation Preview */}
      <section className="p-4 bg-slate-900 border-t border-slate-800">
        <h3 className="text-xs font-bold uppercase tracking-wider text-slate-500 mb-4 flex items-center gap-2">
           <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" /><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" /></svg>
           Visualizer
        </h3>
        <div className="aspect-square bg-slate-950 rounded-2xl overflow-hidden checkered-bg flex items-center justify-center relative shadow-[inset_0_4px_12px_rgba(0,0,0,0.4)] border border-slate-800">
          <Preview project={project} />
        </div>
        <div className="mt-4 p-3 bg-indigo-500/5 rounded-xl border border-indigo-500/10">
           <p className="text-[10px] text-slate-500 leading-relaxed italic">
             Previewing: <span className="text-indigo-400 font-bold">{project.activeAnimation || 'None'}</span>
           </p>
        </div>
      </section>
    </div>
  );
};

export default Sidebar;
