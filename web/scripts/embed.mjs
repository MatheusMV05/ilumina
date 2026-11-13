// Comprime dist/index.html e gera include/DashboardPage.h para o firmware.
// O ESP32 envia os bytes como estão, com "Content-Encoding: gzip".
import { readFileSync, writeFileSync } from "node:fs";
import { gzipSync } from "node:zlib";
import { fileURLToPath } from "node:url";

const root = (p) => fileURLToPath(new URL(`../${p}`, import.meta.url));
const html = readFileSync(root("dist/index.html"));
const gz = gzipSync(html, { level: 9 });

const lines = [];
for (let i = 0; i < gz.length; i += 20) {
  lines.push("  " + Array.from(gz.subarray(i, i + 20), (b) => `0x${b.toString(16).padStart(2, "0")}`).join(", ") + ",");
}

const header = `// Gerado por web/scripts/embed.mjs a partir de web/ (React). Não edite aqui.
// Para regerar: cd web && npm run build
#pragma once

#include <Arduino.h>

// index.html com JS e CSS embutidos, comprimido com gzip (${html.length} bytes sem compressão)
const size_t DASHBOARD_HTML_GZ_LEN = ${gz.length};
const uint8_t DASHBOARD_HTML_GZ[] PROGMEM = {
${lines.join("\n")}
};
`;

writeFileSync(root("../include/DashboardPage.h"), header);
console.log(`DashboardPage.h: ${html.length} B -> ${gz.length} B gzip`);
