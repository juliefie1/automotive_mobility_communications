// -------------------------------------------------------------------------- //
//! @brief main function of the example integrating KIM library and satellite pass
//! prediction library
//!
//! Design is:
//! * get current time through one GPS acquisition
//! * compute next satellite pass
//! * schedule a timer to starting time of next pass
//!
//! @param[in]  argc not used
//! @param[in]  argv not used
//!
//! @returns error status (0: OK, 1 FAIL)
// -------------------------------------------------------------------------- //
LE_SHARED int argos_publisher (void);

