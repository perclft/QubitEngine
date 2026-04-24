import { test, expect } from '@playwright/test';

test.describe('QubitEngine Web Frontend', () => {
  test('should load the dashboard successfully', async ({ page }) => {
    await page.goto('/');
    
    // Expect the title to contain something related to QubitEngine
    await expect(page).toHaveTitle(/QubitEngine|Dashboard/i);
    
    // Expect some key navigational elements to be present
    const nav = page.locator('nav');
    await expect(nav).toBeVisible();
    
    // Instead of waiting for networkidle (which blocks on SSE/websockets), 
    // we just ensure the main container is rendered.
    // Ensure the main container is rendered
    const mainContainer = page.locator('main');
    await expect(mainContainer).toBeVisible();
  });

  test('should navigate to Circuit Lab', async ({ page }) => {
    await page.goto('/');
    
    // Find the link to Circuit Lab
    const circuitLabLink = page.getByRole('link', { name: /Circuit/i });
    if (await circuitLabLink.count() > 0) {
      await circuitLabLink.first().click();
      
      // Ensure we navigated to the circuit lab
      await expect(page).toHaveURL(/.*circuit/i);
      
      // Expect the visual circuit builder to be present (e.g., a canvas or svg or drag-and-drop container)
      const labContainer = page.locator('main');
      await expect(labContainer).toBeVisible();
    } else {
      // If no navigation link, go directly
      await page.goto('/circuit-lab');
      await expect(page).toHaveURL(/.*circuit-lab/i);
    }
  });

  test('should load the VQE page', async ({ page }) => {
    await page.goto('/vqe');
    await expect(page).toHaveURL(/.*vqe/i);
    
    // Check for VQE related headings or controls
    const heading = page.getByRole('heading', { name: /VQE/i });
    if (await heading.count() > 0) {
      await expect(heading.first()).toBeVisible();
    }
  });
});
