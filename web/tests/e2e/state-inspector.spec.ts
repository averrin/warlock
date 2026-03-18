import { test, expect } from "@playwright/test";

test("state inspector panel opens and shows environment", async ({ page }) => {
  await page.goto("/");
  await expect(page.locator("text=conn: connected")).toBeVisible({ timeout: 10000 });

  await page.click("button:has-text('State')");

  await expect(page.locator("text=Environment")).toBeVisible({ timeout: 5000 });
});

test("status bar shows save/load buttons", async ({ page }) => {
  await page.goto("/");
  await expect(page.locator("text=conn: connected")).toBeVisible({ timeout: 10000 });

  await expect(page.locator("button:has-text('Save')")).toBeVisible();
  await expect(page.locator("button:has-text('Load')")).toBeVisible();
  await expect(page.locator("text=Auto")).toBeVisible();
});
