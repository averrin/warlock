import { useState, useEffect } from 'react';

// Keep track of the state from the backend
type GameState = {
  texts: any[];
  lines: any[];
  sprites: any[];
  hitboxes: any[];
};

declare global {
  interface Window {
    updateState: (state: GameState) => void;
  }
}

function App() {
  const [gameState, setGameState] = useState<GameState>({
    texts: [],
    lines: [],
    sprites: [],
    hitboxes: []
  });

  useEffect(() => {
    // Expose the update method to global window so C++ can call it
    window.updateState = (state: GameState) => {
      setGameState(state);
    };
  }, []);

  return (
    <div className="relative w-screen h-screen overflow-hidden bg-neutral-900">
      {/* Background Grid */}
      <div
        className="absolute inset-0 pointer-events-none opacity-20"
        style={{
          backgroundImage: 'radial-gradient(circle, #ffffff 1px, transparent 1px)',
          backgroundSize: '32px 32px'
        }}
      ></div>

      {/* Render connections / lines */}
      <svg className="absolute inset-0 w-full h-full pointer-events-none">
        {gameState.lines.map((line, idx) => (
          <line
            key={`line-${line.id}-${idx}`}
            x1={line.x1}
            y1={line.y1}
            x2={line.x2}
            y2={line.y2}
            stroke={`rgba(${line.color[0]}, ${line.color[1]}, ${line.color[2]}, ${line.color[3] / 255})`}
            strokeWidth={line.thickness || 2}
          />
        ))}
      </svg>

      {/* Render hitboxes as simple rectangles (if needed for debugging) */}
      {gameState.hitboxes.map((hb, idx) => (
        <div
          key={`hb-${hb.id}-${idx}`}
          className="absolute border-2 border-red-500 pointer-events-none"
          style={{
            left: hb.x,
            top: hb.y,
            width: hb.width,
            height: hb.height,
          }}
        />
      ))}

      {/* Render Frames (sprites) as node cards */}
      {gameState.sprites.map((sprite, idx) => {
        const isGhost = sprite.state?.ghost;
        const isSelected = sprite.state?.selected;
        const isHovered = sprite.state?.hovered;

        let borderColor = 'border-neutral-700';
        if (isSelected) borderColor = 'border-red-500';
        else if (isHovered) borderColor = 'border-green-500';

        return (
          <div
            key={`sprite-${sprite.id}-${idx}`}
            className={`absolute flex flex-col justify-center items-center bg-neutral-800 rounded-md border-2 ${borderColor} shadow-lg`}
            style={{
              left: sprite.x,
              top: sprite.y,
              width: sprite.width,
              height: sprite.height,
              transform: `scale(${sprite.scale}) rotate(${sprite.rotation}deg)`,
              opacity: isGhost ? 0.5 : 1,
            }}
          >
            {/* The actual text labels will be layered on top, but we could put an icon here */}
          </div>
        )
      })}

      {/* Render text labels */}
      {gameState.texts.map((text, idx) => (
        <div
          key={`text-${text.id}-${idx}`}
          className="absolute font-semibold pointer-events-none drop-shadow-md"
          style={{
            left: text.x,
            top: text.y,
            color: `rgba(${text.color[0]}, ${text.color[1]}, ${text.color[2]}, ${text.color[3] / 255})`,
            fontSize: `${text.size}px`,
            whiteSpace: 'pre'
          }}
        >
          {text.text}
        </div>
      ))}
    </div>
  );
}

export default App;
