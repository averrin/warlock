import { expect, test } from "@playwright/test";

async function waitForConnected(page: Parameters<typeof test>[0]["page"]) {
  await expect(page.getByText("conn: connected")).toBeVisible();
}

async function ensureFrameExists(page: Parameters<typeof test>[0]["page"]) {
  const frameSelect = page.locator("select").first();
  const optionCount = await frameSelect.locator("option").count();
  if (optionCount > 1) {
    return frameSelect;
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
  return frameSelect;
}

test.describe("frame inspector", () => {
  test("proof: select frame, view components, activate and deactivate", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);

    const frameSelect = await ensureFrameExists(page);
    const firstFrameValue = await frameSelect.locator("option").nth(1).getAttribute("value");
    expect(firstFrameValue).toBeTruthy();
    await frameSelect.selectOption(firstFrameValue ?? "");

    const emptyMessage = page.getByText("No components");
    if (await emptyMessage.isVisible()) {
      const componentSelect = page.locator("#component-select");
      await expect
        .poll(async () => {
          return componentSelect.locator("option").count();
        })
        .toBeGreaterThan(1);
      const componentValue = await componentSelect.locator("option").nth(1).getAttribute("value");
      expect(componentValue).toBeTruthy();
      await componentSelect.selectOption(componentValue ?? "");
      await page.getByRole("button", { name: "Add to Selected Frame" }).click();
      await expect(page.getByText("No components")).toHaveCount(0);
    }

    await page.getByRole("button", { name: "Activate" }).first().click();
    await expect(page.getByText("active").first()).toBeVisible();
    await page.getByRole("button", { name: "Deactivate" }).first().click();
    await expect(page.getByText("inactive").first()).toBeVisible();
  });

  test("regression: selected frame remains selected after refresh", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);

    const frameSelect = await ensureFrameExists(page);
    const firstFrameValue = await frameSelect.locator("option").nth(1).getAttribute("value");
    expect(firstFrameValue).toBeTruthy();
    await frameSelect.selectOption(firstFrameValue ?? "");

    await page.getByRole("button", { name: "Refresh" }).click();
    await expect
      .poll(async () => {
        return frameSelect.inputValue();
      })
      .toBe(firstFrameValue);
  });

  test("temperature tab: shows environment temperature history when available", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);

    const frameSelect = await ensureFrameExists(page);
    const firstFrameValue = await frameSelect.locator("option").nth(1).getAttribute("value");
    expect(firstFrameValue).toBeTruthy();
    await frameSelect.selectOption(firstFrameValue ?? "");

    await page.getByRole("button", { name: "Temperature" }).click();

    await expect(page.getByText("Temperature history")).toBeVisible();
  });
});
