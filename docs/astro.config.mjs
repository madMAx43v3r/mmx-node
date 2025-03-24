// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import rehypeSlug from 'rehype-slug';
import rehypeAutolinkHeadings from 'rehype-autolink-headings';

// https://astro.build/config
export default defineConfig({
	integrations: [
		starlight({
			title: 'Documentation',
			favicon: '/favicon.ico',
			logo: {
				src: './src/assets/logo_text_color_cx256x156_rectangle.png',
			},
			components: {
				SocialIcons: './src/components/SocialIcons.astro',
			},
			social: {
				discord: 'https://discord.gg/BswFhNkMzY',
				github: 'https://github.com/madMAx43v3r/mmx-node',
				'x.com': 'https://x.com/MMX_Network_',
			},
			customCss: [
				'./src/styles/style.css',
			],
			lastUpdated: true,
			editLink: {
				baseUrl: 'https://github.com/madMAx43v3r/mmx-node/edit/master/docs',
			},
			sidebar: [
				{
					label: 'Homepage',
					link: 'https://mmx.network/',
				},
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
	markdown: {
		rehypePlugins: [
			rehypeSlug,
			[rehypeAutolinkHeadings, { behavior: 'append' }]
		],
	},
});
