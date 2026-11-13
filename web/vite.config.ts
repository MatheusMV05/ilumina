import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { viteSingleFile } from "vite-plugin-singlefile";

// O ESP32 serve uma única página, sem internet: JS e CSS vão embutidos no HTML.
export default defineConfig({
  plugins: [react(), viteSingleFile()],
  build: { target: "es2020", assetsInlineLimit: 100_000_000, cssCodeSplit: false },
  server: {
    // `npm run dev` com o ESP32 na rede: VITE_POSTE=http://192.168.4.1 npm run dev
    proxy: process.env.VITE_POSTE ? { "/api": process.env.VITE_POSTE } : undefined,
  },
});
