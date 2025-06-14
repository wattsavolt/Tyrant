#pragma once

/// Prevents deriving classes from being copied
class INonCopyable
{
protected:
	INonCopyable() = default;
	~INonCopyable() = default;

private:
	INonCopyable(const INonCopyable&) = delete;
	INonCopyable& operator=(const INonCopyable&) = delete;
};
