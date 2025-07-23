/** @type {import('tailwindcss').Config} */
export default {
  // This content section is THE MOST IMPORTANT PART.
  // It tells Tailwind to scan all your .vue and .ts files for classes.
  content: [
    "./index.html",
    "./src/**/*.{vue,js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {},
  },
  plugins: [],
}