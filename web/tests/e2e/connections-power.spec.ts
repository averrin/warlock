import { expect, test } from "@playwright/test";

async function waitForConnected(page: Parameters<typeof test>[0]["page"]) {
  await expect(page.getByText("conn: connected")).toBeVisible();
}

async function ensureAtLeastTwoFrames(page: Parameters<typeof test>[0]["page"]) {
  const frameSelect = page.locator("select").first();
  while ((await frameSelect.locator("option").count()) < 3) {
    const canvas = page.locator("canvas").last();
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();
    const count = await frameSelect.locator("option").count();
    const offset = 180 + count * 20;
    await page.mouse.click(Math.round((box?.x ?? 0) + offset), Math.round((box?.y ?? 0) + offset));
    await page.waitForTimeout(100);
  }
}

test.describe("connections power", () => {
  test("proof: create and remove one connection updates state", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);
    await ensureAtLeastTwoFrames(page);

    const source = page.getByLabel("Connection Source");
    const target = page.getByLabel("Connection Target");
    const before = await page.getByRole("button", { name: "Remove" }).count();
    await source.selectOption({ index: 1 });
    await target.selectOption({ index: 2 });
    await page.getByRole("button", { name: "Create Connection" }).click();

    await expect
      .poll(async () => {
        return page.getByRole("button", { name: "Remove" }).count();
      })
      .toBe(before + 1);
    await page.getByRole("button", { name: "Remove" }).first().click();
    await expect
      .poll(async () => {
        return page.getByRole("button", { name: "Remove" }).count();
      })
      .toBe(before);
  });

  test("regression: frame drag and selection stay stable during connection workflow", async ({ page }) => {
    await page.goto("/");
    await waitForConnected(page);
    await ensureAtLeastTwoFrames(page);

    const frameSelect = page.locator("select").first();
    const selectedValue = await frameSelect.locator("option").nth(1).getAttribute("value");
    expect(selectedValue).toBeTruthy();
    await frameSelect.selectOption(selectedValue ?? "");

    const source = page.getByLabel("Connection Source");
    const target = page.getByLabel("Connection Target");
    await source.selectOption({ index: 1 });
    await target.selectOption({ index: 2 });
    await page.getByRole("button", { name: "Create Connection" }).click();
    await page.getByRole("button", { name: "Remove" }).first().click();

    await expect
      .poll(async () => {
        return frameSelect.inputValue();
      })
      .toBe(selectedValue);
  });
});
