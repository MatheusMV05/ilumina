import { useId, type CSSProperties, type ReactNode } from "react";
import type { Icon } from "@phosphor-icons/react";
import { CaretRightIcon } from "@phosphor-icons/react";

interface WidgetProps {
  color: string;
  icon: Icon;
  title: string;
  link?: { label: string; onClick: () => void };
  aside?: ReactNode;
  className?: string;
  id?: string;
  // Gráficos e listas longas ficam melhor sem as ondas coloridas atrás
  waves?: boolean;
  children: ReactNode;
}

// Cartão de vidro no estilo Frutiger: pastilha colorida, título, atalho e ondas da cor do assunto no rodapé
export function Widget({ color, icon: WidgetIcon, title, link, aside, className, id, waves = true, children }: WidgetProps) {
  const headingId = useId();
  return (
    <section
      id={id}
      className={`widget${waves ? " has-waves" : ""}${className ? ` ${className}` : ""}`}
      style={{ "--widget-color": color } as CSSProperties}
      aria-labelledby={headingId}
    >
      <header className="widget-head">
        <span className="chip chip-round" aria-hidden="true"><WidgetIcon weight="fill" /></span>
        <h2 id={headingId} className="widget-title">{title}</h2>
        {aside}
        {link && (
          <button type="button" className="link-btn" onClick={link.onClick}>
            {link.label} <CaretRightIcon weight="bold" aria-hidden="true" />
          </button>
        )}
      </header>
      {children}
    </section>
  );
}
