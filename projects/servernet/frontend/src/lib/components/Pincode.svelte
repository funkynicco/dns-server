<script lang="ts">
    export let length = 4;
    
    export let code = '';
    
    export let nopadding = false;
    
    $: code_array = getCodeArray(code);
    
    function getCodeArray(c: string) {
        let result = [];
        
        for (let i = 0; i < length; i++) {
            if (i < c.length) {
                result.push('*');
            } else {
                result.push(null);
            }
        }
        
        return result;
    }
    
    async function onPadButtonPressed(c: string) {
        if (c >= '0' && c <= '9') {
            if (code.length < length) {
                code += c;
            }
            
            return;
        }
        
        if (c === '<') {
            if (code.length !== 0) {
                code = code.substring(0, code.length - 1);
            }
            
            return;
        }
        
        if (c == 'C') {
            code = '';
        }
    }
</script>

<div class="pincode {nopadding ? '' : 'pincode-padding'}">
    <div class="masked">
        {#each code_array as chr}
            <div class="mask-item">
                {chr ?? ''}
            </div>
        {/each}
    </div>
    <div class="pad">
        <div class="pad-row">
            <div class="pad-btn" on:click={async () => onPadButtonPressed('1')}>1</div>
            <div class="pad-btn" on:click={async () => onPadButtonPressed('2')}>2</div>
            <div class="pad-btn" on:click={async () => onPadButtonPressed('3')}>3</div>
        </div>
        <div class="pad-row">
            <div class="pad-btn" on:click={async () => onPadButtonPressed('4')}>4</div>
            <div class="pad-btn" on:click={async () => onPadButtonPressed('5')}>5</div>
            <div class="pad-btn" on:click={async () => onPadButtonPressed('6')}>6</div>
        </div>
        <div class="pad-row">
            <div class="pad-btn" on:click={async () => onPadButtonPressed('7')}>7</div>
            <div class="pad-btn" on:click={async () => onPadButtonPressed('8')}>8</div>
            <div class="pad-btn" on:click={async () => onPadButtonPressed('9')}>9</div>
        </div>
        <div class="pad-row">
            <div class="pad-btn" on:click={async () => onPadButtonPressed('<')}>&lt;</div>
            <div class="pad-btn" on:click={async () => onPadButtonPressed('0')}>0</div>
            <div class="pad-btn" on:click={async () => onPadButtonPressed('C')}>C</div>
        </div>
    </div>
</div>

<style lang="scss">
    $key-gap: 24px;
    
    .masked {
        padding: 20px;
        margin-bottom: 20px;
        border-radius: 16px;
        display: flex;
        gap: 15px;
        justify-content: center;
    }
    
    .mask-item {
        width: 40px;
        height: 40px;
        border-bottom: 1px solid rgba(0, 0, 0, 0.4);
        text-align: center;
        font-size: 3em;
    }
    
    .pincode-padding {
        padding: 0 180px 0 180px;
    }
    
    .pad-row {
        display: flex;
        justify-content: space-between;
        gap: $key-gap;
        
        &:not(:first-child) {
            margin-top: $key-gap;
        }
    }
    
    .pad-btn {
        flex: 1;
        width: 120px;
        height: 120px;
        background: #f6c394;
        cursor: pointer;
        border-radius: 50%;
        font-family: Overpass;
        font-size: 3em;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);
        
        display: flex;
        align-items: center;
        justify-content: center;
    }
</style>