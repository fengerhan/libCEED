use crate::prelude::*;
use std::convert::TryFrom;

/// CeedVector context wrapper
pub struct Vector<'a> {
    ceed: &'a crate::Ceed,
    pub ptr: bind_ceed::CeedVector,
}

impl<'a> Vector<'a> {
    /// Constructors
    pub fn create(ceed: &'a crate::Ceed, n: usize) -> Self {
        let n = i32::try_from(n).unwrap();
        let mut ptr = std::ptr::null_mut();
        unsafe { bind_ceed::CeedVectorCreate(ceed.ptr, n, &mut ptr) };
        Self { ceed, ptr }
    }

    pub fn new(ceed: &'a crate::Ceed, ptr: bind_ceed::CeedVector) -> Self {
        Self { ceed, ptr }
    }

    /// Create a Vector from a slice
    ///
    /// # arguments
    ///
    /// * 'slice' values to initialize vector with
    ///
    /// ```
    /// # let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
    /// let x = ceed::vector::Vector::from_slice(&ceed, &[1., 2., 3.,]);
    /// assert_eq!(x.length(), 3);
    /// ```
    pub fn from_slice(ceed: &'a crate::Ceed, v: &[f64]) -> Self {
        let mut x = Self::create(ceed, v.len());
        x.set_slice(v);
        x
    }

    /// Returns the length of a CeedVector
    ///
    /// ```
    /// # let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
    /// let vec = ceed.vector(10);
    /// let n = vec.length();
    /// assert_eq!(n, 10);
    /// ```
    pub fn length(&self) -> usize {
        let mut n = 0;
        unsafe { bind_ceed::CeedVectorGetLength(self.ptr, &mut n) };
        usize::try_from(n).unwrap()
    }

    /// Returns the length of a CeedVector
    ///
    /// ```
    /// # let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
    /// let vec = ceed.vector(10);
    /// assert_eq!(vec.len(), 10);
    /// ```
    pub fn len(&self) -> usize {
        self.length()
    }

    /// Set the array used by a CeedVector, freeing any previously allocated
    ///   array if applicable
    ///
    /// # arguments
    ///
    /// * 'mtype' - Memory type of  the array being passed
    /// * 'cmode' - Copy mode for the array
    /// * 'vec'   - Array to be used
    ///
    /// ```
    /// # let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
    /// let vec = ceed.vector(4);
    /// let mut array: [f64; 4] = [1., 2., 3., 4.];
    /// vec.set_array(ceed::MemType::Host, ceed::CopyMode::OwnPointer, array.to_vec());
    /// let norm = vec.norm(ceed::NormType::Max);
    /// assert_eq!(norm, 4.0)
    /// ```
    pub fn set_array(&self, mtype: crate::MemType, cmode: crate::CopyMode, mut vec: Vec<f64>) {
        vec.shrink_to_fit();
        unsafe {
            bind_ceed::CeedVectorSetArray(
                self.ptr,
                mtype as bind_ceed::CeedMemType,
                cmode as bind_ceed::CeedCopyMode,
                vec.as_mut_ptr(),
            )
        };
        if cmode == crate::CopyMode::OwnPointer {
            std::mem::forget(vec);
        }
    }

    /// Set the CeedVector to a constant value
    ///
    /// # arguments
    ///
    /// * 'val' - Value to be used
    ///
    /// ```
    /// let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
    /// let mut x = ceed.vector(10);
    /// x.set_value(42.0);
    /// ```
    pub fn set_value(&mut self, value: f64) {
        unsafe { bind_ceed::CeedVectorSetValue(self.ptr, value) };
    }

    /// Set values from a slice of the same length
    ///
    /// # arguments
    ///
    /// * 'slice' values to into self; length must match
    ///
    /// ```
    /// # let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
    /// let mut x = ceed.vector(4);
    /// x.set_slice(&[10., 11., 12., 13.]);
    /// ```
    pub fn set_slice(&mut self, slice: &[f64]) {
        assert_eq!(self.length(), slice.len());
        unsafe {
            bind_ceed::CeedVectorSetArray(
                self.ptr,
                crate::MemType::Host as bind_ceed::CeedMemType,
                crate::CopyMode::CopyValues as bind_ceed::CeedCopyMode,
                slice.as_ptr() as *mut f64,
            )
        };
    }

    /// Sync the CeedVector to a specified memtype
    ///
    /// # arguments
    ///
    /// * 'mtype' - Memtype to be synced
    ///
    /// ```
    /// # let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
    /// let vec = ceed.vector(10);
    /// vec.sync(ceed::MemType::Host);
    /// ```
    pub fn sync(&self, mtype: crate::MemType) {
        unsafe { bind_ceed::CeedVectorSyncArray(self.ptr, mtype as bind_ceed::CeedMemType) };
    }

    /// Return the norm of a CeedVector
    ///
    /// # arguments
    ///
    /// * 'ntype' - Norm type CEED_NORM_1, CEED_NORM_2, or CEED_NORM_MAX
    ///
    /// ```
    /// # let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
    /// let mut x = ceed.vector(10);
    /// x.set_value(42.0);
    /// let norm = x.norm(ceed::NormType::Max);
    /// assert_eq!(norm, 42.0)
    /// ```
    pub fn norm(&self, ntype: crate::NormType) -> f64 {
        let mut res: f64 = 0.0;
        unsafe { bind_ceed::CeedVectorNorm(self.ptr, ntype as bind_ceed::CeedNormType, &mut res) };
        res
    }
}

/// Destructor
impl<'a> Drop for Vector<'a> {
    fn drop(&mut self) {
        unsafe {
            bind_ceed::CeedVectorDestroy(&mut self.ptr);
        }
    }
}
