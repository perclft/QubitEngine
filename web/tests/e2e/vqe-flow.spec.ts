import { test, expect } from '@playwright/test';

test.describe('VQE Explorer Flow', () => {
  test('should run VQE and display convergence chart', async ({ page }) => {
    // Navigate to the VQE page
    await page.goto('/vqe');

    // Ensure the page has loaded properly
    await expect(page.getByRole('heading', { name: 'VQE Explorer' })).toBeVisible();
    await expect(page.getByRole('button', { name: 'Start VQE' })).toBeVisible();

    // Mock the SSE streaming endpoint to simulate a successful backend run
    await page.route('**/api/vqe/stream*', async (route) => {
      const responseBody = [
        JSON.stringify({ iteration: 1, energy: -0.5 }),
        JSON.stringify({ iteration: 2, energy: -0.8 }),
        JSON.stringify({ iteration: 3, energy: -1.0 }),
        JSON.stringify({ iteration: 4, energy: -1.137, converged: true })
      ].join('\n') + '\n';
      
      await route.fulfill({
        status: 200,
        contentType: 'text/event-stream',
        body: responseBody,
      });
    });

    // Click the "Start VQE" button
    await page.click('button:has-text("Start VQE")');

    // Due to our mocked response, it should quickly converge
    await expect(page.locator('text=Converged!')).toBeVisible({ timeout: 5000 });
    
    // Verify that the final energy is displayed
    await expect(page.locator('text=-1.137000 Ha')).toBeVisible();

    // Ensure chart renders
    await expect(page.locator('.recharts-responsive-container')).toBeVisible();
  });
});
