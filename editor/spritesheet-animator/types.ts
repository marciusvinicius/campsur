
export interface Frame {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface AnimationSet {
  [name: string]: Frame[];
}

export interface ProjectState {
  imageName: string;
  texturePath: string;
  imageUrl: string | null;
  imageWidth: number;
  imageHeight: number;
  frameWidth: number;
  frameHeight: number;
  animations: AnimationSet;
  activeAnimation: string;
}
