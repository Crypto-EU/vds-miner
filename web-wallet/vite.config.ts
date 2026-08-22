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
      "/pool-api": {
        target: "https://server.666pool.com",
        changeOrigin: true,
        secure: true,
        rewrite: (path) => path.replace(/^\/pool-api/, ""),
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
      "/pool-api": {
        target: "https://server.666pool.com",
        changeOrigin: true,
        secure: true,
        rewrite: (path) => path.replace(/^\/pool-api/, ""),
      },
    },
  },
});
