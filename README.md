`TGenerator<T>` is a template class that implements generators in Unreal Engine using native C++20 coroutines.

## Example usage:

```cpp
TGenerator<int> Range(const int Low, const int High)
{
    for (int i = Low; i < High; i++)
        co_yield i;
}
```

## Notes:
- `TGenerator<T>` wraps a `TSharedPtr` to the coroutine's state and can be safely copied by value. Once there are no more `TGenerator<T>` instances, the coroutine is destroyed.
- `TWeakGeneratorHandle<T>` can be used to weakly refer to the coroutine.
- Generator objects are evaluated lazily. Example:
    ```cpp
    auto Values = Range(10, 20);    // Generator is created but not executed yet.

    for (const auto &i : Values)    // Now execute the generator's code.
        UE_LOG(LogTemp, Warning, TEXT("The integer value is: %d"), i);
    ```
- Calling `begin()` or `CreateIterator()` methods on newly created generators will resume them once.
- You may execute the generator manually or with an iterator:
    ```cpp
    // With iterators:
    for (auto i = Values.begin(); i != Values.end(); i++)
        ...

    // Manually:
    while (Values.Resume())
        UE_LOG(LogTemp, Warning, TEXT("The integer value is: %d"), Values.GetCurrentValue());
    ```
- After the generator finishes, it keeps the last value as its current value.