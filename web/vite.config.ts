import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        manualChunks(id) {
          if (id.includes("/node_modules/recharts/")) return "recharts";
          return undefined;
        },
      },
    },
  },
  server: {
    proxy: {
      "/api": "http://127.0.0.1:3001",
    },
  },
});
