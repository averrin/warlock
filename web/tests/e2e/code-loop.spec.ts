import { expect, test } from "@playwright/test";

async function waitForConnected(page: Parameters<typeof test>[0]["page"]) {
  await expect(page.getByText("conn: connected")).toBeVisible();
}

async function ensureFrameExistsAndSelect(page: Parameters<typeof test>[0]["page"]) {
  const frameSelect = page.locator("select").first();
  const optionCount = await frameSelect.locator("option").count();
  if (optionCount <= 1) {
    const canvas = page.locator("canvas");
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();
    await page.mouse.click(Math.round((box?.x ?? 0) + 220), Math.round((box?.y ?? 0) + 220));
    await expect
      .poll(async () => {
        return frameSelect.locator("option").count();
      })
      .toBeGreaterThan(1);
  }
  const firstFrameValue = await frameSelect.locator("option").nth(1).getAttribute("value");
  expect(firstFrameValue).toBeTruthy();
  await frameSelect.selectOption(firstFrameValue ?? "");
}

test.describe("code loop", () => {
  test("proof: load, edit, save, execute update shows success", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);
    await ensureFrameExistsAndSelect(page);

    await page.getByRole("button", { name: "Load Selected Frame Script" }).click();
    await page.getByRole("button", { name: "Save to Selected Frame Core" }).click();
    await page.getByRole("button", { name: "Run update" }).click();
    await expect(page.getByText("Run: ok")).toBeVisible();
  });

  test("regression: save still works when execute capability is unavailable", async ({ page }) => {
    await page.addInitScript(() => {
      (window as { __warlockUnsupportedMethods?: string[] }).__warlockUnsupportedMethods = ["code.execute"];
    });
    await page.goto("/");
    await waitForConnected(page);
    await ensureFrameExistsAndSelect(page);

    await page.getByRole("button", { name: "Load Selected Frame Script" }).click();
    await page.getByRole("button", { name: "Save to Selected Frame Core" }).click();
    await expect(page.getByText("Saved core script")).toBeVisible();
    await expect(page.getByRole("button", { name: "Run update" })).toBeDisabled();
  });
});
