import type { StatusSpec } from "../sensors";

// Orbe de vidro com reflexo: o ícone carrega o significado, a cor reforça
export function Orb({ spec, size = "md" }: { spec: Pick<StatusSpec, "tone" | "icon">; size?: "sm" | "md" | "lg" }) {
  const Icon = spec.icon;
  return (
    <span className={`orb orb-${size} tone-${spec.tone}`} aria-hidden="true">
      <Icon weight="bold" />
    </span>
  );
}

export function Badge({ spec, label }: { spec: StatusSpec; label?: string }) {
  return (
    <span className={`badge tone-${spec.tone}`}>
      <Orb spec={spec} size="sm" />
      <span>{label ?? spec.text}</span>
    </span>
  );
}
