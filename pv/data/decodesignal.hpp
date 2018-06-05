/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2017 Soeren Apel <soeren@apelpie.net>
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

#ifndef PULSEVIEW_PV_DATA_DECODESIGNAL_HPP
#define PULSEVIEW_PV_DATA_DECODESIGNAL_HPP

#include <atomic>
#include <condition_variable>
#include <unordered_set>
#include <vector>

#include <QSettings>
#include <QString>

#include <libsigrokdecode/libsigrokdecode.h>

#include <pv/data/decode/row.hpp>
#include <pv/data/decode/rowdata.hpp>
#include <pv/data/signalbase.hpp>
#include <pv/util.hpp>

using std::atomic;
using std::condition_variable;
using std::map;
using std::mutex;
using std::pair;
using std::vector;
using std::shared_ptr;

namespace pv {
class Session;

namespace data {

namespace decode {
class Annotation;
class Decoder;
class Row;
}

class Logic;
class LogicSegment;
class SignalBase;
class SignalData;

struct DecodeChannel
{
	uint16_t id;     ///< Global numerical ID for the decode channels in the stack
	uint16_t bit_id; ///< Tells which bit within a sample represents this channel
	const bool is_optional;
	const pv::data::SignalBase *assigned_signal;
	const QString name, desc;
	int initial_pin_state;
	const shared_ptr<pv::data::decode::Decoder> decoder_;
	const srd_channel *pdch_;
};

struct DecodeSegment
{
	map<const decode::Row, decode::RowData> annotation_rows;
	pv::util::Timestamp start_time;
	double samplerate;
	int64_t samples_decoded_incl, samples_decoded_excl;
};

class DecodeSignal : public SignalBase
{
	Q_OBJECT

private:
	static const double DecodeMargin;
	static const double DecodeThreshold;
	static const int64_t DecodeChunkLength;

public:
	DecodeSignal(pv::Session &session);
	virtual ~DecodeSignal();

	bool is_decode_signal() const;
	const vector< shared_ptr<data::decode::Decoder> >& decoder_stack() const;

	void stack_decoder(const srd_decoder *decoder);
	void remove_decoder(int index);
	bool toggle_decoder_visibility(int index);

	void reset_decode(bool shutting_down = false);
	void begin_decode();
	QString error_message() const;

	const vector<data::DecodeChannel> get_channels() const;
	void auto_assign_signals(const shared_ptr<pv::data::decode::Decoder> dec);
	void assign_signal(const uint16_t channel_id, const SignalBase *signal);
	int get_assigned_signal_count() const;

	void set_initial_pin_state(const uint16_t channel_id, const int init_state);

	double samplerate() const;
	const pv::util::Timestamp start_time() const;

	/**
	 * Returns the number of samples that can be worked on,
	 * i.e. the number of samples where samples are available
	 * for all connected channels.
	 */
	int64_t get_working_sample_count(uint32_t segment_id) const;

	int64_t get_decoded_sample_count_min(uint32_t segment_id) const;

	/**
	 * Returns the number of processed samples. Newly generated annotations will
	 * have sample numbers greater than this.
	 *
	 * If include_processing is true, this number will include the ones being
	 * currently processed (in case the decoder stack is running). In this case,
	 * newly generated annotations will have sample numbers smaller than this.
	 */
	int64_t get_decoded_sample_count(uint32_t segment_id,
		bool include_processing) const;

	vector<decode::Row> visible_rows() const;

	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		vector<pv::data::decode::Annotation> &dest,
		const decode::Row &row, uint32_t segment_id, uint64_t start_sample,
		uint64_t end_sample) const;

	virtual void save_settings(QSettings &settings) const;

	virtual void restore_settings(QSettings &settings);

private:
	void set_error_message(QString msg);

	uint32_t get_input_segment_count() const;

	uint32_t get_input_samplerate(uint32_t segment_id) const;

	void update_channel_list();

	void commit_decoder_channels();

	void mux_logic_samples(uint32_t segment_id, const int64_t start, const int64_t end);

	void logic_mux_proc();

	void decode_data(const int64_t abs_start_samplenum, const int64_t sample_count,
		const shared_ptr<LogicSegment> input_segment);

	void decode_proc();

	void start_srd_session();
	void terminate_srd_session();
	void stop_srd_session();

	void connect_input_notifiers();

	void create_decode_segment();

	static void annotation_callback(srd_proto_data *pdata, void *decode_signal);

Q_SIGNALS:
	void new_annotations(); // TODO Supply segment for which they belong to
	void decode_reset();
	void decode_finished();
	void channels_updated();

private Q_SLOTS:
	void on_capture_state_changed(int state);
	void on_data_cleared();
	void on_data_received();

private:
	pv::Session &session_;

	vector<data::DecodeChannel> channels_;

	struct srd_session *srd_session_;

	shared_ptr<Logic> logic_mux_data_;
	uint32_t logic_mux_unit_size_;
	bool logic_mux_data_invalid_;

	vector< shared_ptr<decode::Decoder> > stack_;
	bool stack_config_changed_;
	map<pair<const srd_decoder*, int>, decode::Row> class_rows_;

	vector<DecodeSegment> segments_;
	uint32_t current_segment_id_;

	mutable mutex input_mutex_, output_mutex_, logic_mux_mutex_;
	mutable condition_variable decode_input_cond_, logic_mux_cond_;

	std::thread decode_thread_, logic_mux_thread_;
	atomic<bool> decode_interrupt_, logic_mux_interrupt_;

	QString error_message_;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODESIGNAL_HPP
