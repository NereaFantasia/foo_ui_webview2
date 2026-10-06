import { defineConfig } from 'vite';

export default defineConfig({
  server: {
    port: 5173,
    // foobar2000 loads the address typed into its preferences; if the port is
    // taken, fail instead of moving to another one.
    strictPort: true,
  },
});
