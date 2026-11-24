import { useEffect, useState } from "react";
import { LightbulbFilamentIcon } from "@phosphor-icons/react";

const SEEN_KEY = "intro-vista";

// A abertura roda uma vez por sessão do navegador e some com movimento reduzido
function shouldPlay() {
  if (matchMedia("(prefers-reduced-motion: reduce)").matches) return false;
  try {
    return sessionStorage.getItem(SEEN_KEY) === null;
  } catch {
    return true;
  }
}

const PLAY = shouldPlay();
// Os painéis esperam a abertura terminar para entrar (ver .reveal em styles.css)
if (PLAY) document.documentElement.dataset.intro = "on";

// Abertura: o poste "acende" (orbe sobe, lâmpada brilha, nome aparece) e o céu se abre para o painel
export function Intro() {
  const [phase, setPhase] = useState<"play" | "leave" | "done">(PLAY ? "play" : "done");

  useEffect(() => {
    if (phase !== "play") return;
    try { sessionStorage.setItem(SEEN_KEY, "1"); } catch { /* sem armazenamento: só repete a abertura */ }
    const skip = () => {
      // Os painéis começam a subir enquanto o céu da abertura ainda se desfaz
      delete document.documentElement.dataset.intro;
      setPhase("leave");
    };
    const timer = setTimeout(skip, 1900);
    addEventListener("keydown", skip);
    return () => {
      clearTimeout(timer);
      removeEventListener("keydown", skip);
    };
  }, [phase]);

  if (phase === "done") return null;

  return (
    <div
      className={`intro${phase === "leave" ? " is-leaving" : ""}`}
      aria-hidden="true"
      onClick={() => {
        delete document.documentElement.dataset.intro;
        setPhase("leave");
      }}
      onAnimationEnd={(e) => {
        if (e.target === e.currentTarget && phase === "leave") setPhase("done");
      }}
    >
      <div className="intro-stage">
        <span className="intro-halo" />
        <span className="intro-ring" />
        <span className="intro-orb"><LightbulbFilamentIcon weight="duotone" /></span>
        <p className="intro-title">
          {"Iluminação Pública IoT".split(" ").map((w, i) => (
            <span key={w} style={{ animationDelay: `${0.55 + i * 0.08}s` }}>{w}</span>
          ))}
        </p>
        <p className="intro-sub">Conectando ao poste</p>
      </div>
    </div>
  );
}
