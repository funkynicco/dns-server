import type {LayoutLoad} from "../../.svelte-kit/types/src/routes/$types";
import {goto} from "$app/navigation";

export const load: LayoutLoad = async ({route, session, fetch, url}: any) => {
    
    return { // page data
        url: url.pathname
    };
};

export const ssr = false;