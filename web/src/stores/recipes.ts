import { create } from "zustand";

export interface RecipeDTO {
  name: string;
  /** Present when server sends `recipes.list` with power data; used for modifier presets. */
  power_cost?: number;
  available: string[];
  outputs: { name: string; amount: number }[];
  inputs: { name: string; amount: number }[];
}

interface RecipeStore {
  recipes: RecipeDTO[];
  setRecipes: (recipes: RecipeDTO[]) => void;
}

export const useRecipeStore = create<RecipeStore>((set) => ({
  recipes: [],
  setRecipes: (recipes) => set({ recipes }),
}));
