import { expect, test } from "@playwright/test";

test.describe("smoke cutover", () => {
  test("connect, claim, frame workflow, component/code mutation", async ({ page }) => {
    await page.goto("/");
    await expect(page.getByText("conn: connected")).toBeVisible();
    await expect(page.getByText("claimed: yes")).toBeVisible();

    const frameSelect = page.locator("select").first();
    await expect
      .poll(async () => {
        return frameSelect.locator("option").count();
      })
      .toBeGreaterThan(1);

    const selectedValue = await frameSelect.locator("option").nth(1).getAttribute("value");
    expect(selectedValue).toBeTruthy();
    await frameSelect.selectOption(selectedValue ?? "");

    const canvas = page.locator("canvas").last();
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();
    const x = Math.round((box?.x ?? 0) + 256);
    const y = Math.round((box?.y ?? 0) + 256);
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + 128, y, { steps: 8 });
    await page.mouse.up();

    await page.getByRole("button", { name: "Activate" }).first().click();
    await expect(page.getByText("active").first()).toBeVisible();

    await page.getByRole("button", { name: "Load Selected Frame Script" }).click();
    await page.getByRole("button", { name: "Save to Selected Frame Core" }).click();
    await expect(page.getByText("Saved core script")).toBeVisible();
  });
});
