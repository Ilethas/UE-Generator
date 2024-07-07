// MIT License
//
// Copyright © 2024 Mariusz Kurowski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "CoreMinimal.h"
#include <coroutine>

#ifndef PLATFORM_EXCEPTIONS_DISABLED
#include <exception>
#endif

template <typename TFrom, typename TTo>
concept CanBeYieldedFromGenerator = requires(TTo* To, TFrom From)
{
	To = std::addressof(From);
};

template <typename T>
class TGenerator;

template <typename T>
struct TGeneratorPromise;

template <typename T>
class TGeneratorIterator;

template <typename T>
class TWeakGeneratorHandle
{
protected:
	TWeakPtr<TGeneratorPromise<T>> Promise = nullptr;

public:
	TWeakGeneratorHandle() = default;

	explicit TWeakGeneratorHandle(const TGenerator<T>& Generator) : Promise(Generator.Promise)
	{
	}

	TGenerator<T> Pin() const noexcept;
	operator TGenerator<T>() const noexcept { return Pin(); }
};

template <typename T>
TGenerator<T> TWeakGeneratorHandle<T>::Pin() const noexcept
{
	if (Promise.IsValid())
		return TGenerator<T>{Promise.Pin()};
	return TGenerator<T>{};
}

template <typename T>
class TGenerator
{
	friend TGeneratorPromise<T>;
	friend TWeakGeneratorHandle<T>;
	friend TGeneratorIterator<T>;

public:
	using promise_type = TGeneratorPromise<T>;
	using iterator = TGeneratorIterator<T>;

	T* GetCurrentValuePtr() const noexcept { return Promise->CurrentValue; }
	T& GetCurrentValue() const noexcept;
	bool HasValue() const noexcept { return Promise->CurrentValue != nullptr; }

	/** Check if the generator has already finished execution (i.e. reached final_suspend). */
	bool IsDone() const noexcept { return Promise->IsDone(); }

	/** Resume and return true if the generator is not finished yet. */
	bool Resume();

	iterator begin();
	static iterator end() noexcept { return {}; }
	iterator CreateIterator() noexcept { return begin(); }

	TWeakGeneratorHandle<T> GetWeakHandle() const noexcept { return TWeakGeneratorHandle<T>{*this}; }

private:
	TSharedPtr<TGeneratorPromise<T>> Promise;

	TGenerator(TGeneratorPromise<T>* Promise);
};

template <typename T>
T& TGenerator<T>::GetCurrentValue() const noexcept
{
	checkf(Promise && Promise->CurrentValuePtr, TEXT("Attempted to access an empty generator"));
	return *Promise->CurrentValuePtr;
}

template <typename T>
bool TGenerator<T>::Resume()
{
	const bool Result = Promise->Resume();
#ifndef PLATFORM_EXCEPTIONS_DISABLED
	Promise->ThrowIfException();
#endif
	return Result;
}

template <typename T>
typename TGenerator<T>::iterator TGenerator<T>::begin()
{
	if (!HasValue()) Promise->Resume();
#ifndef PLATFORM_EXCEPTIONS_DISABLED
	Promise->ThrowIfException();
#endif
	return iterator(*this);
}

template <typename T>
TGenerator<T>::TGenerator(TGeneratorPromise<T>* Promise)
	: Promise(Promise, [](auto Promise) { Promise->Destroy(); })
{
}

template <typename T>
struct TGeneratorPromise
{
	T* CurrentValuePtr = nullptr;

#ifndef PLATFORM_EXCEPTIONS_DISABLED
	std::exception_ptr Exception;
	void ThrowIfException() const { if (Exception) std::rethrow_exception(Exception); }
#endif

	TGenerator<T> get_return_object() { return this; }
	std::suspend_always initial_suspend() { return {}; }
	std::suspend_always final_suspend() noexcept { return {}; }

	template <CanBeYieldedFromGenerator<T> TFrom>
	std::suspend_always yield_value(TFrom&& YieldedValue);

	void return_void()
	{
	}

	void unhandled_exception();

	template <typename U>
	std::suspend_never await_transform(U&&) = delete;

	bool Resume();
	bool IsDone() { return GetHandle().done(); }
	void Destroy() { GetHandle().destroy(); }

	std::coroutine_handle<TGeneratorPromise> GetHandle() noexcept
	{
		return std::coroutine_handle<TGeneratorPromise>::from_promise(*this);
	}
};

template <typename T>
template <CanBeYieldedFromGenerator<T> TFrom>
std::suspend_always TGeneratorPromise<T>::yield_value(TFrom&& YieldedValue)
{
	CurrentValuePtr = std::addressof(YieldedValue);
	return {};
}

template <typename T>
void TGeneratorPromise<T>::unhandled_exception()
{
#ifndef PLATFORM_EXCEPTIONS_DISABLED
	Exception = std::current_exception();
#endif
}

template <typename T>
bool TGeneratorPromise<T>::Resume()
{
	auto Handle = GetHandle();
	if (!Handle.done()) Handle.resume();
	return !Handle.done();
}

template <typename T>
class TGeneratorIterator : public TWeakGeneratorHandle<T>
{
public:
	using TWeakGeneratorHandle<T>::TWeakGeneratorHandle;

	TGeneratorIterator& operator++()
	{
		checkf(this->Promise.IsValid(), TEXT("Attempted to advance an invalid iterator"));
		if (auto StrongPromise = this->Promise.Pin(); !StrongPromise->Resume())
		{
			this->Promise = nullptr;
#ifndef PLATFORM_EXCEPTIONS_DISABLED
			StrongPromise->ThrowIfException();
#endif
		}
		return *this;
	}

	void operator++(int) { operator++(); }

	T& operator*() const noexcept
	{
		checkf(this->Promise.IsValid(), TEXT("Attempted to dereference an invalid iterator"));
		return *this->Promise.Pin()->CurrentValue;
	}

	T* operator->() const noexcept { return std::addressof(operator*()); }

	bool operator==(const TGeneratorIterator& Other) const noexcept { return this->Promise == Other.Promise; }
	bool operator!=(const TGeneratorIterator& Other) const noexcept { return this->Promise != Other.Promise; }
	explicit operator bool() const noexcept { return this->Promise.IsValid(); }
};
