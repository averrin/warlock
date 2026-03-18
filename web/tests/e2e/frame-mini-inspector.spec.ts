import { expect, test } from "@playwright/test";

async function waitForConnected(page: Parameters<typeof test>[0]["page"]) {
  await expect(page.getByText("conn: connected")).toBeVisible();
}

async function ensureFrameExists(page: Parameters<typeof test>[0]["page"]) {
  const frameSelect = page.locator("select").first();
  const optionCount = await frameSelect.locator("option").count();
  if (optionCount > 1) {
    return;
  }

  const canvas = page.locator("canvas").last();
  const box = await canvas.boundingBox();
  expect(box).not.toBeNull();
  await page.mouse.click(Math.round((box?.x ?? 0) + 220), Math.round((box?.y ?? 0) + 220));
  await expect
    .poll(async () => {
      return frameSelect.locator("option").count();
    })
    .toBeGreaterThan(1);
}

test.describe("frame mini inspector", () => {
  test("proof: clicking a frame opens mini inspector window", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);
    await ensureFrameExists(page);

    const canvas = page.locator("canvas").last();
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();

    // Click a likely frame area.
    await page.mouse.click(Math.round((box?.x ?? 0) + 220), Math.round((box?.y ?? 0) + 220));

    await expect(page.getByText("Frame Mini Inspector")).toBeVisible();
  });
});

