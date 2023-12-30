import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';
export default defineConfig({
	plugins: [sveltekit()],
	server: {
		proxy: {
			'/api': 'http://localhost:5000',
			'/admin/login': 'http://localhost:5000',
			'/account': 'http://localhost:5000',
			'/signin-oidc': 'http://localhost:5000',
			'/MicrosoftIdentity': 'http://localhost:5000',
			'/_framework': 'http://localhost:5000',
		}
	}
});
