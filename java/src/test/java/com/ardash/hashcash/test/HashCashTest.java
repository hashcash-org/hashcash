package com.ardash.hashcash.test;

/*
 Copyright 2016 Andreas Redmer <a_r@posteo.de>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 */

import java.security.NoSuchAlgorithmException;

import junit.framework.Assert;

import org.junit.Test;

import com.ardash.hashcash.HashCash;

public class HashCashTest {

	@Test
	public void test() throws NoSuchAlgorithmException {
		//HashCash hc = new HashCash("")
		HashCash hc = HashCash.mintCash("foo@bar", 20,1);
		System.out.println(hc);
//		hc.estimateTime(value)
//		fail("Not yet implemented");
	}
	
	@Test
	public void verifytest() throws NoSuchAlgorithmException {
		HashCash hc = HashCash.parseFromString("1:48:110416:etienne@cri.fr:::000A2F00000063BF012");
		
		System.out.println(hc);
		System.out.println(hc.getValue());
		System.out.println(hc.getHashSumHex());
		System.out.println(hc.isValid());
		
//		hc.estimateTime(value)
//		fail("Not yet implemented");
	}

	@Test
	public void nondeterministicMintAndVerify() {
		for (int j = 0; j < 20; j++) {
			for (int i = 0; i < 20; i++) {
				HashCash hc = HashCash.mintCash("foo@bar", i,1);
				Assert.assertTrue("garbage minted "+ hc, hc.isValid());
			}
		}
//		hc.estimateTime(value)
//		fail("Not yet implemented");
	}

}
