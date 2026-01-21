
import React from 'react';

interface HeaderProps {
  imageName: string;
  onUpload: (e: React.ChangeEvent<HTMLInputElement>) => void;
  onExport: () => void;
}

const Header: React.FC<HeaderProps> = ({ imageName, onUpload, onExport }) => {
  return (
    <header className="h-16 px-6 bg-slate-800 border-b border-slate-700 flex items-center justify-between shadow-lg z-10">
      <div className="flex items-center gap-4">
        <div className="bg-indigo-600 p-2 rounded-lg">
          <svg className="w-6 h-6 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" />
          </svg>
        </div>
        <div>
          <h1 className="text-lg font-bold leading-none">SpriteSheet Animator</h1>
          <p className="text-xs text-slate-400">{imageName || 'No image loaded'}</p>
        </div>
      </div>

      <div className="flex items-center gap-3">
        <label className="flex items-center gap-2 px-4 py-2 bg-slate-700 hover:bg-slate-600 rounded-md cursor-pointer transition-colors text-sm font-medium">
          <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" />
          </svg>
          Upload Sprite
          <input type="file" className="hidden" accept="image/jpeg,image/png" onChange={onUpload} />
        </label>
        
        <button 
          onClick={onExport}
          className="flex items-center gap-2 px-4 py-2 bg-indigo-600 hover:bg-indigo-500 rounded-md transition-colors text-sm font-medium disabled:opacity-50 disabled:cursor-not-allowed"
          disabled={!imageName}
        >
          <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 10v6m0 0l-3-3m3 3l3-3m2 8H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
          </svg>
          Export JSON
        </button>
      </div>
    </header>
  );
};

export default Header;
