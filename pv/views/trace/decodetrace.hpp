/*
 * This file is part of the PulseView project.
 *
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

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_DECODETRACE_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_DECODETRACE_HPP

#include "trace.hpp"

#include <list>
#include <map>
#include <memory>
#include <vector>

#include <QColor>
#include <QComboBox>
#include <QSignalMapper>
#include <QTimer>

#include <pv/binding/decoder.hpp>
#include <pv/data/decode/annotation.hpp>
#include <pv/data/decode/row.hpp>
#include <pv/data/signalbase.hpp>

using std::list;
using std::map;
using std::pair;
using std::shared_ptr;
using std::vector;
using pv::data::decode::Annotation;

struct srd_channel;
struct srd_decoder;

namespace pv {

class Session;

namespace data {
struct DecodeChannel;
class DecodeSignal;

namespace decode {
class Decoder;
}
}  // namespace data

namespace widgets {
class DecoderGroupBox;
}

namespace views {
namespace trace {

class DecodeTrace : public Trace
{
	Q_OBJECT

private:
	typedef struct {
		qreal abs_start, abs_end;
		QColor color;
		bool block_class_uniform;
		shared_ptr<Annotation> ann;
	} CachedAnnotation;

	typedef struct {
		data::decode::Row decoder_row;
		vector<CachedAnnotation> ann_cache;
		int title_width;
		QColor color;
	} RowInfo;

	static const QColor ErrorBgColor;
	static const QColor NoDecodeColor;

	static const int ArrowSize;
	static const double EndCapWidth;
	static const int RowTitleMargin;
	static const int DrawPadding;

	static const int MaxTraceUpdateRate;

public:
	DecodeTrace(pv::Session &session, shared_ptr<data::SignalBase> signalbase,
		int index);

	bool enabled() const;

	shared_ptr<data::SignalBase> base() const;

	/**
	 * Computes the vertical extents of the contents of this row item.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	pair<int, int> v_extents() const;

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with..
	 */
	void paint_back(QPainter &p, ViewItemPaintParams &pp);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_mid(QPainter &p, ViewItemPaintParams &pp);

	/**
	 * Paints the foreground layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_fore(QPainter &p, ViewItemPaintParams &pp);

	void populate_popup_form(QWidget *parent, QFormLayout *form);

	QMenu* create_context_menu(QWidget *parent);

	void delete_pressed();

private:
	void cache_annotation(RowInfo *row_info, qreal abs_start, qreal abs_end,
		QColor color, bool block_class_uniform, Annotation *ann = nullptr);

	void build_annotation_cache(RowInfo *row_info,
		vector<Annotation> annotations, QPainter &p);

	void draw_annotations(RowInfo *row_info, QPainter &p,
		const ViewItemPaintParams &pp, int y);

	void draw_annotation(shared_ptr<Annotation> a, QPainter &p,
		qreal start, qreal end, int y, const ViewItemPaintParams &pp,
		QColor color, int row_title_width) const;

	void draw_annotation_block(qreal start, qreal end,
		bool block_class_uniform, QPainter &p, int y, QColor color) const;

	void draw_instant(shared_ptr<Annotation> a, QPainter &p,
		qreal x, int y) const;

	void draw_range(shared_ptr<Annotation> a, QPainter &p,
		qreal start, qreal end, int y, const ViewItemPaintParams &pp,
		int row_title_width) const;

	void draw_error(QPainter &p, const QString &message,
		const ViewItemPaintParams &pp);

	void draw_unresolved_period(QPainter &p, int left, int right) const;

	pair<double, double> get_pixels_offset_samples_per_pixel() const;

	/**
	 * Determines the start and end sample for a given pixel range.
	 * @param x_start the X coordinate of the start sample in the view
	 * @param x_end the X coordinate of the end sample in the view
	 * @return Returns a pair containing the start sample and the end
	 * 	sample that correspond to the start and end coordinates.
	 */
	pair<uint64_t, uint64_t> get_sample_range(int x_start, int x_end) const;

	QColor get_row_color(int row_index) const;
	QColor get_annotation_color(QColor row_color, int annotation_index) const;

	int get_row_at_point(const QPoint &point);

	const QString get_annotation_at_point(const QPoint &point);

	void create_decoder_form(int index,
		shared_ptr<pv::data::decode::Decoder> &dec,
		QWidget *parent, QFormLayout *form);

	QComboBox* create_channel_selector(QWidget *parent,
		const data::DecodeChannel *ch);
	QComboBox* create_channel_selector_init_state(QWidget *parent,
		const data::DecodeChannel *ch);

public:
	void hover_point_changed(const QPoint &hp);

private Q_SLOTS:
	void on_new_annotations();
	void on_delayed_trace_update();
	void on_decode_reset();
	void on_decode_finished();

	void on_delete();

	void on_channel_selected(int);

	void on_channels_updated();

	void on_init_state_changed(int);

	void on_stack_decoder(srd_decoder *decoder);

	void on_delete_decoder(int index);

	void on_show_hide_decoder(int index);

private:
	pv::Session &session_;
	shared_ptr<data::DecodeSignal> decode_signal_;
	vector<RowInfo> rows_;

	map<QComboBox*, uint16_t> channel_id_map_;  // channel selector -> decode channel ID
	map<QComboBox*, uint16_t> init_state_map_;  // init state selector -> decode channel ID
	list< shared_ptr<pv::binding::Decoder> > bindings_;

	vector<pv::widgets::DecoderGroupBox*> decoder_forms_;

	int row_height_, ann_height_;

	int min_useful_label_width_;

	QSignalMapper delete_mapper_, show_hide_mapper_;

	QTimer delayed_trace_updater_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_DECODETRACE_HPP
