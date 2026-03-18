import { expect, test } from "@playwright/test";

test("environment floating window exposes speed controls and charts", async ({ page }) => {
  await page.goto("/");
  await expect(page.getByText("conn: connected")).toBeVisible();

  const time = page.getByTestId("env-time");
  await expect(time).toBeVisible();
  const initialTime = await time.textContent();

  await page.getByLabel("speed-x10").click();
  await expect
    .poll(async () => {
      return await time.textContent();
    })
    .not.toBe(initialTime);

  await page.getByLabel("speed-pause").click();
  const pausedTime = await time.textContent();
  await page.waitForTimeout(1200);
  await expect(time).toHaveText(pausedTime ?? "");

  await expect(page.locator(".recharts-surface").first()).toBeVisible();
});
