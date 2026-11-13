// Gera dist/prototipo.html: a mesma página sem <html>/<head>/<body>, para publicar
// numa hospedagem que já monta esse esqueleto (o dist/index.html continua completo).
import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

const path = (p) => fileURLToPath(new URL(`../${p}`, import.meta.url));
const html = readFileSync(path("dist/index.html"), "utf8");

const head = html.match(/<head>([\s\S]*?)<\/head>/)[1]
  .replace(/<meta charset[^>]*>\s*/, "")
  .replace(/<meta name="viewport"[^>]*>\s*/, "")
  .replace(/<title>[^<]*<\/title>/, "");
const body = html.match(/<body>([\s\S]*?)<\/body>/)[1];

const page = `<title>Painel do Poste IoT</title>
<meta name="claude:viewport-fit-sides" content="cover">
${head.trim()}
${body.trim()}
`;
writeFileSync(path("dist/prototipo.html"), page);
console.log(`prototipo.html: ${page.length} B`);
