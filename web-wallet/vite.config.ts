import { defineConfig } from "vite";

export default defineConfig({
  server: {
    host: "0.0.0.0",
    port: 43187,
    strictPort: true,
    proxy: {
      "/vds-api": {
        target: "https://www.vdscool.com",
        changeOrigin: true,
        secure: true,
      },
    },
  },
  preview: {
    host: "0.0.0.0",
    port: 43187,
    strictPort: true,
    proxy: {
      "/vds-api": {
        target: "https://www.vdscool.com",
        changeOrigin: true,
        secure: true,
      },
    },
  },
});
