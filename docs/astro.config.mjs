// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import rehypeSlug from 'rehype-slug';
import rehypeAutolinkHeadings from 'rehype-autolink-headings';
import starlightSidebarTopics from 'starlight-sidebar-topics';

// https://astro.build/config
export default defineConfig({
	integrations: [
		starlight({
			title: 'Docs',
			favicon: '/favicon.ico',
			logo: {
				src: './src/assets/logo_text_color_cx256x156_rectangle.png',
			},
			components: {
				MarkdownContent: './src/components/MarkdownContent.astro',
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
			plugins: [
				starlightSidebarTopics([
					{
						label: 'Documentation',
						link: '/guides/getting-started',
						icon: 'open-book',
						items: [
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
					},
					{
						label: 'Articles',
						link: '/articles/general/mmx-tldr',
						icon: 'document',
						items: [
							{
								label: 'General',
								autogenerate: { directory: 'articles/general' },
							},
							{
								label: 'Plotting',
								autogenerate: { directory: 'articles/plotting' },
							},
							{
								label: 'TimeLord',
								autogenerate: { directory: 'articles/timelord' },
							},
							{
								label: 'Wallets',
								autogenerate: { directory: 'articles/wallets' },
							},
						],
					},
				]),
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
