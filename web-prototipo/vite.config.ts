import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { viteSingleFile } from "vite-plugin-singlefile";

// Uma página só, com JS e CSS embutidos, fácil de hospedar ou mandar como arquivo
export default defineConfig({
  plugins: [react(), viteSingleFile()],
  build: { target: "es2020", assetsInlineLimit: 100_000_000, cssCodeSplit: false },
});
