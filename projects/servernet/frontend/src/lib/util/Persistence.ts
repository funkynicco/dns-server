import {writable} from "svelte/store";
import {browser} from "$app/environment";

const LocalStorageKey = 'trivs_persistence';

interface IPersistence {
    api_key: string | undefined;
    last_error: string | undefined;
    return_url: string | undefined;
}

const defaultValue = JSON.stringify({
    api_key: undefined, 
    last_error: undefined,
    return_url: undefined
});

// load from local storage (if found); otherwise use defaultValue above
const initialValue = JSON.parse(browser ? window.localStorage.getItem(LocalStorageKey) ?? defaultValue : defaultValue);

export const persistence = writable<IPersistence>(initialValue);

export const unauthorized_admin = writable<boolean>(true);

// every time the store is updated; we flush changes to the browser local storage
persistence.subscribe(value => {
    if (browser) {
        window.localStorage.setItem(LocalStorageKey, JSON.stringify(value));
    }
});