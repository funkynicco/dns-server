<script lang="ts">
    import {goto} from "$app/navigation";
    import {onMount} from "svelte";
    import AsyncDataLoader from "$lib/components/AsyncDataLoader.svelte";

    export let theme = 'yellow';
    
    export let shadow = true;
    
    export let arrow = 'right';
    
    let holdFingerTimer: number | undefined;
    
    export let holdDelay = 5000;
    
    export let holdCallback: (() => Promise<void>) | undefined = undefined;
    
    onMount(() => {       
        return () => cancelHoldFingerTimer();
    });
    
    function onMouseDown() {
        holdFingerTimer = setTimeout(async () => {
            if (holdCallback) {
                await holdCallback();
            }
        }, holdDelay);
    }
    
    function cancelHoldFingerTimer() {
        if (holdFingerTimer) {
            clearTimeout(holdFingerTimer);
            holdFingerTimer = undefined;
        }
    }
    
    function onMouseUp() {
        cancelHoldFingerTimer();
    }
    
    function onMouseLeave() {
        cancelHoldFingerTimer();
    }
</script>

<button class="btn btn-{theme} btn-arrow-{arrow} {shadow ? 'btn-shadow' : ''}" on:click on:mousedown={onMouseDown} on:mouseup={onMouseUp} on:mouseleave={onMouseLeave}>
    <slot></slot>
</button>

<style lang="scss">
    .btn {
        display: inline-block;
        color: #000;
        text-align: left;
        font-size: 20px;
        font-weight: 600;
        padding: 16px 20px 16px 20px;
        line-height: 1.22;
        font-family: 'Overpass',sans-serif;
        position: relative;
        cursor: pointer;
    }

    .btn-yellow {
        border: 2px solid #fbe122;
        background-color: #fbe122;
    }

    .btn-gray {
        border: 2px solid #b0aa7e;
        background-color: #b0aa7e;
    }
    
    .btn-arrow-left {
        padding-left: 90px;
        
        &::before {
            content: "";
            background-image: url(https://nordlo.com/wp-content/uploads/2019/09/arrow-big.svg);
            background-repeat: no-repeat;
            background-size: contain;
            background-position: center;
            position: absolute;
            left: 25px;
            top: 19px;
            width: 46px;
            height: 21px;
            display: inline-block;
            transform: scaleX(-1);
        }
    }

    .btn-arrow-right {
        padding-right: 90px;
        
        &::before {
            content: "";
            background-image: url(https://nordlo.com/wp-content/uploads/2019/09/arrow-big.svg);
            background-repeat: no-repeat;
            background-size: contain;
            background-position: center;
            position: absolute;
            right: 25px;
            top: 19px;
            width: 46px;
            height: 21px;
            display: inline-block;
        }
    }
    
    .btn-shadow {
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);
    }
</style>