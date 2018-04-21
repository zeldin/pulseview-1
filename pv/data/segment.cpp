/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2017 Soeren Apel <soeren@apelpie.net>
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "segment.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>

using std::lock_guard;
using std::min;
using std::recursive_mutex;

namespace pv {
namespace data {

const uint64_t Segment::MaxChunkSize = 10 * 1024 * 1024;  /* 10MiB */

Segment::Segment(uint32_t segment_id, uint64_t samplerate, unsigned int unit_size) :
	segment_id_(segment_id),
	sample_count_(0),
	start_time_(0),
	samplerate_(samplerate),
	unit_size_(unit_size),
	is_complete_(false)
{
	lock_guard<recursive_mutex> lock(mutex_);
	assert(unit_size_ > 0);

	// Determine the number of samples we can fit in one chunk
	// without exceeding MaxChunkSize
	chunk_size_ = min(MaxChunkSize, (MaxChunkSize / unit_size_) * unit_size_);

	// Create the initial chunk
	current_chunk_ = new uint8_t[chunk_size_];
	data_chunks_.push_back(current_chunk_);
	used_samples_ = 0;
	unused_samples_ = chunk_size_ / unit_size_;
}

Segment::~Segment()
{
	lock_guard<recursive_mutex> lock(mutex_);

	for (uint8_t* chunk : data_chunks_)
		delete[] chunk;
}

uint64_t Segment::get_sample_count() const
{
	lock_guard<recursive_mutex> lock(mutex_);
	return sample_count_;
}

const pv::util::Timestamp& Segment::start_time() const
{
	return start_time_;
}

double Segment::samplerate() const
{
	return samplerate_;
}

void Segment::set_samplerate(double samplerate)
{
	samplerate_ = samplerate;
}

unsigned int Segment::unit_size() const
{
	return unit_size_;
}

uint32_t Segment::segment_id() const
{
	return segment_id_;
}

void Segment::set_complete()
{
	is_complete_ = true;
}

bool Segment::is_complete() const
{
	return is_complete_;
}

void Segment::append_single_sample(void *data)
{
	lock_guard<recursive_mutex> lock(mutex_);

	// There will always be space for at least one sample in
	// the current chunk, so we do not need to test for space

	memcpy(current_chunk_ + (used_samples_ * unit_size_), data, unit_size_);
	used_samples_++;
	unused_samples_--;

	if (unused_samples_ == 0) {
		current_chunk_ = new uint8_t[chunk_size_];
		data_chunks_.push_back(current_chunk_);
		used_samples_ = 0;
		unused_samples_ = chunk_size_ / unit_size_;
	}

	sample_count_++;
}

void Segment::append_samples(void* data, uint64_t samples)
{
	lock_guard<recursive_mutex> lock(mutex_);

	const uint8_t* data_byte_ptr = (uint8_t*)data;
	uint64_t remaining_samples = samples;
	uint64_t data_offset = 0;

	do {
		uint64_t copy_count = 0;

		if (remaining_samples <= unused_samples_) {
			// All samples fit into the current chunk
			copy_count = remaining_samples;
		} else {
			// Only a part of the samples fit, fill up current chunk
			copy_count = unused_samples_;
		}

		const uint8_t* dest = &(current_chunk_[used_samples_ * unit_size_]);
		const uint8_t* src = &(data_byte_ptr[data_offset]);
		memcpy((void*)dest, (void*)src, (copy_count * unit_size_));

		used_samples_ += copy_count;
		unused_samples_ -= copy_count;
		remaining_samples -= copy_count;
		data_offset += (copy_count * unit_size_);

		if (unused_samples_ == 0) {
			// If we're out of memory, this will throw std::bad_alloc
			current_chunk_ = new uint8_t[chunk_size_];
			data_chunks_.push_back(current_chunk_);
			used_samples_ = 0;
			unused_samples_ = chunk_size_ / unit_size_;
		}
	} while (remaining_samples > 0);

	sample_count_ += samples;
}

void Segment::get_raw_samples(uint64_t start, uint64_t count,
	uint8_t* dest) const
{
	assert(start < sample_count_);
	assert(start + count <= sample_count_);
	assert(count > 0);
	assert(dest != nullptr);

	lock_guard<recursive_mutex> lock(mutex_);

	uint8_t* dest_ptr = dest;

	uint64_t chunk_num = (start * unit_size_) / chunk_size_;
	uint64_t chunk_offs = (start * unit_size_) % chunk_size_;

	while (count > 0) {
		const uint8_t* chunk = data_chunks_[chunk_num];

		uint64_t copy_size = min(count * unit_size_,
			chunk_size_ - chunk_offs);

		memcpy(dest_ptr, chunk + chunk_offs, copy_size);

		dest_ptr += copy_size;
		count -= (copy_size / unit_size_);

		chunk_num++;
		chunk_offs = 0;
	}
}

} // namespace data
} // namespace pv
