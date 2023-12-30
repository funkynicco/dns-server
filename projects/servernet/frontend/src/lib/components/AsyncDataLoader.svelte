<script lang="ts">
    import {fade} from "svelte/transition";
    import {onMount} from "svelte";
    import Loader from "$lib/components/Loader.svelte";
    import Button from "$lib/components/Button.svelte";
    import {goto} from "$app/navigation";
    import TimeoutHandler from "$lib/components/TimeoutHandler.svelte";
    import {ComponentMode} from "$lib/util/Defines";
    import {UnauthorizedError} from "$lib/util/ApiHelper";
    import Pincode from "$lib/components/Pincode.svelte";
    import {unauthorized_admin} from "$lib/util/Persistence";

    let componentMode = ComponentMode.Loading;
    let errorMessage = '';
    let errorIsUnauthorizedAdmin = false;

    export let data: () => Promise<any>;

    export let result: any;

    export let onStateChanged: ((state: ComponentMode) => Promise<void>) | undefined = undefined;

    export let admin = false;

    onMount(async () => {
        errorIsUnauthorizedAdmin = false;
        $unauthorized_admin = false;

        try {
            result = await data();
        } catch (ex) {
            if (ex instanceof UnauthorizedError) {
                if (admin) {
                    //location.href = '/admin/login';
                    errorIsUnauthorizedAdmin = true;
                    $unauthorized_admin = true;
                } else {
                    await goto('/');
                }
            }
            if (ex instanceof Error) {
                errorMessage = ex.toString();
            } else {
                errorMessage = 'An error occurred loading the component.';
            }

            componentMode = ComponentMode.Error;
            if (onStateChanged) {
                await onStateChanged(componentMode);
            }
            return;
        }

        componentMode = ComponentMode.Completed;
        if (onStateChanged) {
            await onStateChanged(componentMode);
        }
    });
</script>

{#key componentMode}
    <div in:fade={{ duration: 150, delay: 150 }}
         out:fade={{ duration: 150 }}>
        {#if componentMode === ComponentMode.Completed}
            <slot></slot>
        {:else if componentMode === ComponentMode.Error}
            <div class={!errorIsUnauthorizedAdmin ? 'error-msg' : 'unauthorized-admin'}>
                {#if !admin}
                    <TimeoutHandler/>
                {/if}
                {#if errorIsUnauthorizedAdmin}
                    <p>Denna sida kräver inloggning.</p>
                    <Button on:click={async () => await goto('/admin/login')}>Logga in</Button>
                {:else}
                    <p>{errorMessage}</p>
                    <Button arrow="left" on:click={async () => await goto('/')}>Tillbaka</Button>
                {/if}
            </div>
        {:else}
            <Loader center={true}/>
        {/if}
    </div>
{/key}

<style lang="scss">
    .error-msg {
        text-align: center;
        background: rgba(200, 0, 0, 0.5);
        box-shadow: 0 0 15px rgba(255, 0, 0, 0.6);
        padding: 30px 0 50px 0;
        margin-top: calc(1080px / 2 - 100px);

        & > p {
            color: #eee;
        }
    }
    
    .unauthorized-admin {
        text-align: center;
    }
</style>