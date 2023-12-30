import {persistence} from "$lib/util/Persistence";
import {get} from "svelte/store";

export class ApiError extends Error {
    public api_message: string;

    constructor(text: string, api_message: string) {
        super(text);
        Object.setPrototypeOf(this,  ApiError.prototype);

        this.api_message = api_message;
    }
}

export class UnauthorizedError extends Error {
    constructor(text: string) {
        super(text);
        Object.setPrototypeOf(this,  UnauthorizedError.prototype);
    }
}

export class ApiHelper {
    private static makeHeaders(extra?: object): unknown {
        if (!extra) {
            extra = {};
        }
        
        const api_key = get(persistence).api_key;
        if (api_key) {
            return {
                'Authorization': `Bearer ${api_key}`,
                ...extra
            };
        }

        return extra;
    }
    
    static async getAsync<T>(url: string, fetch: any): Promise<T | null> {
        const response = await fetch(url, {
            headers: this.makeHeaders(),
            credentials: 'include'
        });

        if (!response.ok) {
            if (response.status === 401 ||
                response.status === 403) {
                throw new UnauthorizedError('Unauthorized');
            }
            
            throw new Error('Get request failed');
        }

        const json = await response.json();
        if (json.result !== 'ok') {
            throw new ApiError(`Get request failed with result: ${json.result}`, json.message);
        }

        return json.data as T;
    }
    
    static async postAsync<T>(url: string, model: T, fetch: any): Promise<void> {
        const response = await fetch(url, {
            headers: this.makeHeaders({
                'Content-Type': 'application/json'
            }),
            method: 'POST',
            credentials: 'include',
            body: JSON.stringify(model)
        });

        if (!response.ok) {
            if (response.status === 401 ||
                response.status === 403) {
                throw new UnauthorizedError('Unauthorized');
            }
            
            throw new Error('Post request failed');
        }

        const json = await response.json();
        if (json.result !== 'ok') {
            throw new ApiError(`Post request failed with result: ${json.result}`, json.message);
        }
    }

    static async postWithResultAsync<TResult, TModel>(url: string, model: TModel, fetch: any): Promise<TResult> {
        const response = await fetch(url, {
            headers: this.makeHeaders({
                'Content-Type': 'application/json'
            }),
            method: 'POST',
            credentials: 'include',
            body: JSON.stringify(model)
        });

        if (!response.ok) {
            if (response.status === 401 ||
                response.status === 403) {
                throw new UnauthorizedError('Unauthorized');
            }
            
            throw new Error('Post request failed');
        }

        const json = await response.json();
        if (json.result !== 'ok') {
            throw new ApiError(`Post request failed with result: ${json.result}`, json.message);
        }

        return json.data as TResult;
    }

    static async deleteAsync(url: string, fetch: any): Promise<void> {
        const response = await fetch(url, {
            headers: this.makeHeaders(),
            credentials: 'include',
            method: 'DELETE'
        });

        if (!response.ok) {
            if (response.status === 401 ||
                response.status === 403) {
                throw new UnauthorizedError('Unauthorized');
            }
            
            throw new Error('Delete request failed');
        }

        const json = await response.json();
        if (json.result !== 'ok') {
            throw new ApiError(`Delete request failed with result: ${json.result}`, json.message);
        }
    }
}