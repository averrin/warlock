import { expect, test } from "@playwright/test";

const GRID = 128;

async function waitForConnected(page: Parameters<typeof test>[0]["page"]) {
  await expect(page.getByText("conn: connected")).toBeVisible();
}

async function frameMoveCount(page: Parameters<typeof test>[0]["page"]) {
  return page.evaluate(() => {
    const sent = (window as { __rpcMethods?: string[] }).__rpcMethods ?? [];
    return sent.filter((method) => method === "frame.move").length;
  });
}

test.describe("frame interaction", () => {
  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      const methods: string[] = [];
      const originalSend = WebSocket.prototype.send;
      WebSocket.prototype.send = function patchedSend(data: string | ArrayBufferLike | Blob | ArrayBufferView) {
        if (typeof data === "string") {
          try {
            const parsed = JSON.parse(data) as { method?: unknown };
            if (typeof parsed.method === "string") {
              methods.push(parsed.method);
            }
          } catch {
            // Ignore non-JSON payloads.
          }
        }
        return originalSend.call(this, data);
      };
      (window as { __rpcMethods?: string[] }).__rpcMethods = methods;
    });
  });

  test("proof: app connects, frame list appears, and drag emits one move", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);

    const frameSelect = page.locator("select").first();
    await expect(frameSelect).toBeVisible();

    const canvas = page.locator("canvas");
    await expect(canvas).toBeVisible();
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();

    const createLocalX = 200;
    const createLocalY = 200;
    const startX = Math.round((box?.x ?? 0) + createLocalX);
    const startY = Math.round((box?.y ?? 0) + createLocalY);
    await page.mouse.click(startX, startY);

    await expect
      .poll(async () => {
        return frameSelect.locator("option").count();
      })
      .toBeGreaterThan(1);

    const dragStartX = Math.round((box?.x ?? 0) + Math.round(createLocalX / GRID) * GRID + 12);
    const dragStartY = Math.round((box?.y ?? 0) + Math.round(createLocalY / GRID) * GRID + 12);
    const before = await frameMoveCount(page);
    await page.mouse.move(dragStartX, dragStartY);
    await page.mouse.down();
    await page.mouse.move(dragStartX + 128, dragStartY, { steps: 8 });
    await page.mouse.up();

    await expect.poll(async () => frameMoveCount(page)).toBe(before + 1);
  });

  test("regression: hover-only pointer movement does not emit frame.move", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);

    const canvas = page.locator("canvas");
    await expect(canvas).toBeVisible();
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();

    const frameSelect = page.locator("select").first();
    const createLocalX = 260;
    const createLocalY = 260;
    const before = await frameMoveCount(page);
    const hoverX = Math.round((box?.x ?? 0) + createLocalX);
    const hoverY = Math.round((box?.y ?? 0) + createLocalY);
    await page.mouse.click(hoverX, hoverY);

    await expect
      .poll(async () => {
        return frameSelect.locator("option").count();
      })
      .toBeGreaterThan(1);

    await page.mouse.move(hoverX, hoverY);
    await page.mouse.move(hoverX + 80, hoverY + 16, { steps: 8 });
    const frameX = Math.round((box?.x ?? 0) + Math.round(createLocalX / GRID) * GRID + 12);
    const frameY = Math.round((box?.y ?? 0) + Math.round(createLocalY / GRID) * GRID + 12);
    await page.mouse.move(frameX, frameY);
    await page.mouse.down();
    await page.mouse.up();

    await expect.poll(async () => frameMoveCount(page)).toBe(before);
  });
});
