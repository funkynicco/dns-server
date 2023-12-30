export class TaskHelper {
    static delayAsync(ms: number): Promise<void> {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
}