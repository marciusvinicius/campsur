
import React, { useState, useEffect, useCallback, useRef } from 'react';
import { ProjectState, AnimationSet, Frame } from './types';
import Editor from './components/Editor';
import Sidebar from './components/Sidebar';
import Header from './components/Header';

const App: React.FC = () => {
  const [project, setProject] = useState<ProjectState>({
    imageName: '',
    texturePath: 'assets/textures/spritesheet.png',
    imageUrl: null,
    imageWidth: 0,
    imageHeight: 0,
    frameWidth: 64,
    frameHeight: 128,
    animations: {
      'idleDown': []
    },
    activeAnimation: 'idleDown'
  });

  const handleImageUpload = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (file) {
      const url = URL.createObjectURL(file);
      const img = new Image();
      img.onload = () => {
        setProject(prev => ({
          ...prev,
          imageName: file.name,
          imageUrl: url,
          imageWidth: img.naturalWidth,
          imageHeight: img.naturalHeight,
          // Default to a reasonable guess if not set
          frameWidth: prev.frameWidth || 64,
          frameHeight: prev.frameHeight || 64
        }));
      };
      img.src = url;
    }
  };

  const updateFrameSize = (width: number, height: number) => {
    setProject(prev => ({ ...prev, frameWidth: width, frameHeight: height }));
  };

  const updateTexturePath = (path: string) => {
    setProject(prev => ({ ...prev, texturePath: path }));
  };

  const addAnimation = (name: string) => {
    if (!name || project.animations[name]) return;
    setProject(prev => ({
      ...prev,
      animations: { ...prev.animations, [name]: [] },
      activeAnimation: name
    }));
  };

  const renameAnimation = (oldName: string, newName: string) => {
    if (!newName || newName === oldName || project.animations[newName]) return;
    setProject(prev => {
      const newAnims = { ...prev.animations };
      newAnims[newName] = newAnims[oldName];
      delete newAnims[oldName];
      return {
        ...prev,
        animations: newAnims,
        activeAnimation: prev.activeAnimation === oldName ? newName : prev.activeAnimation
      };
    });
  };

  const deleteAnimation = (name: string) => {
    setProject(prev => {
      const newAnims = { ...prev.animations };
      delete newAnims[name];
      const remaining = Object.keys(newAnims);
      return {
        ...prev,
        animations: newAnims,
        activeAnimation: remaining.length > 0 ? remaining[0] : ''
      };
    });
  };

  const setActiveAnimation = (name: string) => {
    setProject(prev => ({ ...prev, activeAnimation: name }));
  };

  const addFrame = useCallback((frame: Frame) => {
    if (!project.activeAnimation) return;
    setProject(prev => ({
      ...prev,
      animations: {
        ...prev.animations,
        [prev.activeAnimation]: [...prev.animations[prev.activeAnimation], frame]
      }
    }));
  }, [project.activeAnimation]);

  const removeFrame = (animName: string, index: number) => {
    setProject(prev => {
      const updatedFrames = [...prev.animations[animName]];
      updatedFrames.splice(index, 1);
      return {
        ...prev,
        animations: {
          ...prev.animations,
          [animName]: updatedFrames
        }
      };
    });
  };

  const exportJson = useCallback(() => {
    if (!project.imageName) return;
    const data = {
      texture: project.texturePath,
      image_name: project.imageName,
      image_width: project.imageWidth,
      image_height: project.imageHeight,
      animations: project.animations
    };
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${project.imageName.split('.')[0] || 'animations'}.json`;
    a.click();
    URL.revokeObjectURL(url);
  }, [project]);

  // Global Keyboard Shortcuts (Save)
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
        e.preventDefault();
        exportJson();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [exportJson]);

  return (
    <div className="flex flex-col h-screen w-screen bg-slate-900 text-slate-100 font-sans">
      <Header 
        imageName={project.imageName} 
        onUpload={handleImageUpload} 
        onExport={exportJson}
      />
      
      <main className="flex flex-1 overflow-hidden">
        <div className="flex-1 overflow-auto bg-slate-950 relative border-r border-slate-800">
          <Editor 
            project={project} 
            onAddFrame={addFrame} 
            onUpdateFrameSize={updateFrameSize}
          />
        </div>
        
        <aside className="w-80 overflow-y-auto bg-slate-900 border-l border-slate-800 flex flex-col custom-scrollbar">
          <Sidebar 
            project={project}
            onUpdateSize={updateFrameSize}
            onUpdateTexturePath={updateTexturePath}
            onAddAnim={addAnimation}
            onRenameAnim={renameAnimation}
            onDeleteAnim={deleteAnimation}
            onSelectAnim={setActiveAnimation}
            onRemoveFrame={removeFrame}
          />
        </aside>
      </main>
    </div>
  );
};

export default App;
