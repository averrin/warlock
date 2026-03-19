import { defineConfig } from "@playwright/test";

export default defineConfig({
  testDir: "./tests/e2e",
  timeout: 60_000,
  workers: 1,
  fullyParallel: false,
  use: {
    baseURL: "http://127.0.0.1:4173",
    headless: true,
  },
  webServer: [
    {
      command: "just run --ui-mode web --rpc-port 9810",
      port: 9810,
      cwd: "..",
      reuseExistingServer: false,
      timeout: 120_000,
    },
    {
      command: "VITE_RPC_URL=ws://127.0.0.1:9810 npm run dev -- --host 127.0.0.1 --port 4173",
      url: "http://127.0.0.1:4173",
      reuseExistingServer: true,
      timeout: 60_000,
    },
  ],
});
