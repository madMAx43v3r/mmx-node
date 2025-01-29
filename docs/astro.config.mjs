// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

// https://astro.build/config
export default defineConfig({
	integrations: [
		starlight({
			title: 'MMX Docs',
			favicon: '/favicon.ico',
			social: {
				github: 'https://github.com/madMAx43v3r/mmx-node',
			},
			customCss: ['./src/styles/style.css'],
			sidebar: [
				{
					label: 'Guides',
					autogenerate: { directory: 'guides' },
				},
				{
					label: 'Software Reference',
					autogenerate: { directory: 'software' },
				},
				{
					label: 'Technical Reference',
					autogenerate: { directory: 'reference' },
				},
				{
					label: 'FAQ',
					autogenerate: { directory: 'faq' },
				},
			],
		}),
	],
});
