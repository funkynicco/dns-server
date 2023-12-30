<script lang="ts">
    import {tweened} from 'svelte/motion';
    import {cubicOut} from 'svelte/easing';

    const progress = tweened(0, {
        duration: 200,
        easing: cubicOut
    });
    
    export let value: string | undefined = undefined;
    
    export let placeholder = '';

    let sub;

    progress.subscribe(r => {
        if (sub) {
            sub.style.top = `calc(20px - ${25 * r}px)`;
            //sub.style.fontSize = `${1 + (1 - r) * 0.2}em`;
        }
    });

    $: value ? progress.set(1) : progress.set(0);
</script>

<div class="text-container">
    <input bind:value class={`${placeholder ? 'tb-margin-16' : ''}`} />
    {#if placeholder}
        <sub bind:this={sub}>{placeholder}</sub>
    {/if}
</div>

<style lang="scss">
    input {
        width: 100%;
        height: 48px;
        background: transparent;
        outline: none;
        border: 0;
        font-family: Overpass, sans-serif;
        font-size: 1.4em;
        color: #fff;
        border-bottom: 3px solid #d1e5d9;
    }

    ::placeholder {
        color: #ccc;
        opacity: 1; /* Firefox */
    }

    ::-ms-input-placeholder { /* Edge 12 -18 */
        color: #ccc;
    }

    .tb-margin-16 {
        margin-top: 16pt;
    }

    .text-container {
        position: relative;
    }

    sub {
        position: absolute;
        left: 2px;
        top: 20px;
        font-size: 1.6em;
        color: rgba(238, 238, 238, 0.459);
        pointer-events: none;
    }
</style>