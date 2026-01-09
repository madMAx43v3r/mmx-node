import { defineCollection, z } from 'astro:content';
import { docsLoader } from '@astrojs/starlight/loaders';
import { docsSchema } from '@astrojs/starlight/schema';

export const collections = {
	docs: defineCollection({
		loader: docsLoader(),
		schema: docsSchema({
			extend: z.object({
				articlePublished: z.date().optional(),
				articleUpdated: z.date().optional(),
				authorName: z.string().optional(),
				authorPicture: z.string().optional(),
				authorURL: z.string().optional(),
			}),
		}),
	}),
};
