// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import rehypeSlug from 'rehype-slug';
import rehypeAutolinkHeadings from 'rehype-autolink-headings';
import starlightSidebarTopics from 'starlight-sidebar-topics';

// https://astro.build/config
export default defineConfig({
	site: 'https://docs.mmx.network',
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
			social: [
				{ icon: 'discord', label: 'Discord', href: 'https://discord.gg/BswFhNkMzY' },
				{ icon: 'github', label: 'GitHub', href: 'https://github.com/madMAx43v3r/mmx-node' },
				{ icon: 'x.com', label: 'X', href: 'https://x.com/MMX_Network_' },
			],
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
						link: '/articles/general/mmx-whitepaper',
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
			[rehypeAutolinkHeadings, { behavior: 'wrap', properties: { className: ['sl-custom-rehype-heading'] } }]
		],
	},
});
