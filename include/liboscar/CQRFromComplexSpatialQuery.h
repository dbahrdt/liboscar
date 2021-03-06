#ifndef LIBOSCAR_CQR_FROM_COMPLEX_SPATIAL_QUERY_H
#define LIBOSCAR_CQR_FROM_COMPLEX_SPATIAL_QUERY_H
#include <sserialize/spatial/CellQueryResult.h>
#include <sserialize/Static/GeoHierarchySubGraph.h>
#include "CQRFromPolygon.h"

namespace liboscar {
namespace detail {

class CQRFromComplexSpatialQuery;

}//end namespace detail

class CQRFromComplexSpatialQuery final {
public:
	enum UnaryOp : uint32_t {UO_INVALID=0, UO_NORTH_OF, UO_EAST_OF, UO_SOUTH_OF, UO_WEST_OF};
	enum BinaryOp : uint32_t {BO_INVALID=0, BO_BETWEEN};
public:
	CQRFromComplexSpatialQuery(const CQRFromComplexSpatialQuery & other);
	CQRFromComplexSpatialQuery(const sserialize::spatial::GeoHierarchySubGraph & ssc, const CQRFromPolygon & cqrfp);
	~CQRFromComplexSpatialQuery();
	sserialize::CellQueryResult compassOp(const sserialize::CellQueryResult & cqr, UnaryOp direction, uint32_t threadCount) const;
	sserialize::CellQueryResult relevantElementOp(const sserialize::CellQueryResult & cqr) const;
	sserialize::CellQueryResult betweenOp(const sserialize::CellQueryResult & cqr1, const sserialize::CellQueryResult & cqr2, uint32_t threadCount) const;
	const liboscar::CQRFromPolygon & cqrfp() const;
private:
	sserialize::RCPtrWrapper<detail::CQRFromComplexSpatialQuery> m_priv;
};

namespace detail {

class CQRFromComplexSpatialQuery: public sserialize::RefCountObject {
public:
	typedef sserialize::Static::spatial::GeoHierarchy::SubSet SubSet;
	using CellInfo = sserialize::CellQueryResult::CellInfo;
private:
	enum QueryItemType : uint32_t { QIT_INVALID=0, QIT_ITEM=1, QIT_REGION=2};
public:
	CQRFromComplexSpatialQuery(const sserialize::spatial::GeoHierarchySubGraph& ssc, const liboscar::CQRFromPolygon& cqrfp);
	virtual ~CQRFromComplexSpatialQuery();
	sserialize::CellQueryResult relevantElementOp(const sserialize::CellQueryResult& cqr) const;
	sserialize::CellQueryResult compassOp(const sserialize::CellQueryResult& cqr, liboscar::CQRFromComplexSpatialQuery::UnaryOp direction, uint32_t threadCount) const;
	sserialize::CellQueryResult betweenOp(const sserialize::CellQueryResult & cqr1, const sserialize::CellQueryResult & cqr2, uint32_t threadCount) const;
	const liboscar::CQRFromPolygon & cqrfp() const;
public: //cqr creation
	//uses auto-detection of accuracy
	sserialize::CellQueryResult cqrFromPolygon(const sserialize::spatial::GeoPolygon & gp, int cqrFlags, uint32_t threadCount) const;
private://polygon creation functions for betweenOp
	///between 0->360, north is at 0
	double bearing(double fromLat, double fromLon, double toLat, double toLon) const;
	void normalize(std::vector<sserialize::spatial::GeoPoint> & gp) const;
	void createPolygon(const sserialize::spatial::GeoPoint& p1, const sserialize::spatial::GeoPoint& p2, std::vector< sserialize::spatial::GeoPoint >& pp) const;
	void createPolygon(const sserialize::Static::spatial::GeoWay & gw1, const sserialize::Static::spatial::GeoWay & gw2, std::vector< sserialize::spatial::GeoPoint >& pp) const;
	void createPolygon(const sserialize::spatial::GeoRect & rect1, const sserialize::spatial::GeoRect & rect2, std::vector<sserialize::spatial::GeoPoint> & gp) const;
	void createPolygon(const sserialize::Static::spatial::GeoWay & gw, const sserialize::Static::spatial::GeoPoint & gp, std::vector< sserialize::spatial::GeoPoint >& pp) const;
	void createPolygon(const sserialize::spatial::GeoRect & polyRect, const sserialize::spatial::GeoPoint & point, std::vector< sserialize::spatial::GeoPoint >& pp) const;
	void createPolygon(const sserialize::spatial::GeoRect & polyRect, const sserialize::Static::spatial::GeoWay & gw, std::vector< sserialize::spatial::GeoPoint >& pp) const;
private: //polygon creation functions for compassOp
	void createPolygon(const sserialize::spatial::GeoPoint & point, double distance, liboscar::CQRFromComplexSpatialQuery::UnaryOp direction, std::vector< sserialize::spatial::GeoPoint >& pp) const;
	void createPolygon(const sserialize::Static::spatial::GeoWay & way, liboscar::CQRFromComplexSpatialQuery::UnaryOp direction, std::vector< sserialize::spatial::GeoPoint >& pp) const;
	void createPolygon(const sserialize::spatial::GeoRect & rect, liboscar::CQRFromComplexSpatialQuery::UnaryOp direction, std::vector< sserialize::spatial::GeoPoint >& pp) const;
private:
	SubSet createSubSet(const sserialize::CellQueryResult cqr) const;
	SubSet::NodePtr determineRelevantRegion(const SubSet & subset) const;
	///@return itemId
	uint32_t determineRelevantItem(const SubSet & subSet, const SubSet::NodePtr & rPtr) const;
	uint32_t determineRelevantItem(const sserialize::ItemIndex & items) const;
private:
	///if qit == QIT_REGION then id is ghId
	void determineQueryItemType(const sserialize::CellQueryResult& cqr, QueryItemType& qit, uint32_t & id) const;
	void determineQueryItemTypeOld(const sserialize::CellQueryResult & cqr, QueryItemType & qit, uint32_t & id) const;
	
private: //accessor function
	const liboscar::Static::OsmKeyValueObjectStore & store() const;
	const CellInfo & cellInfo() const;
	const sserialize::Static::spatial::GeoHierarchy & geoHierarchy() const;
	const sserialize::Static::ItemIndexStore & idxStore() const;
private:
	sserialize::spatial::GeoHierarchySubGraph m_ssc;
	liboscar::CQRFromPolygon m_cqrfp;
	uint32_t m_itemQueryItemCountTh;
	uint32_t m_itemQueryCellCountTh;
};

}}//end namespace liboscar::detail

#endif
